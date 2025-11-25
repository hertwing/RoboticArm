#include "CvWorker.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>

#include <opencv2/imgproc.hpp>
#include <opencv2/objdetect.hpp>
#include <opencv2/video/tracking.hpp>

#include <QElapsedTimer>
#include <QFile>
#include <QSize>
#include <QStandardPaths>
#include <QString>

// local helpers
namespace {
static inline QRect clampTo(const QRect & r, int W, int H)
{
    int x = std::max(0, r.x());
    int y = std::max(0, r.y());
    int w = std::min(r.width(),  W - x);
    int h = std::min(r.height(), H - y);
    return QRect(x, y, w, h);
}

static inline QRect growRect(const QRect & r, double k, int W, int H)
{
    const int nw = int(std::round(r.width()  * k));
    const int nh = int(std::round(r.height() * k));
    const int cx = r.x() + r.width()/2;
    const int cy = r.y() + r.height()/2;
    return clampTo(QRect(cx - nw/2, cy - nh/2, nw, nh), W, H);
}

static inline cv::Rect toCv(const QRect & q)
{
    return cv::Rect(q.x(), q.y(), q.width(), q.height());
}

static inline QRect fromCv(const cv::Rect & r)
{
    return QRect(r.x, r.y, r.width, r.height);
}


// IoU of two rectangles (0..1)
static inline double iou(const QRect& a, const QRect& b)
{
    const int x1 = std::max(a.left(),   b.left());
    const int y1 = std::max(a.top(),    b.top());
    const int x2 = std::min(a.right(),  b.right());
    const int y2 = std::min(a.bottom(), b.bottom());
    const int iw = std::max(0, x2 - x1 + 1);
    const int ih = std::max(0, y2 - y1 + 1);
    const int inter = iw * ih;
    const int au = a.width()*a.height() + b.width()*b.height() - inter;
    return au > 0 ? double(inter)/double(au) : 0.0;
}

// rectangle mask with margin (0..~0.45)
static cv::Mat rectMask(int W, int H, const QRect& r, double marginFrac=0.1)
{
    cv::Mat m(H, W, CV_8U, cv::Scalar(0));
    if (r.isNull()) return m;
    int mx = int(r.width() * marginFrac);
    int my = int(r.height()* marginFrac);
    QRect inner(r.x()+mx, r.y()+my, r.width()-2*mx, r.height()-2*my);
    inner = clampTo(inner, W, H);
    cv::rectangle(m, toCv(inner), cv::Scalar(255), cv::FILLED);
    return m;
}

// median of flow vectors
static cv::Point2f medianFlow(const std::vector<cv::Point2f>& a,
                              const std::vector<cv::Point2f>& b,
                              const std::vector<uchar>& inl) {
    std::vector<float> dx, dy; dx.reserve(a.size()); dy.reserve(a.size());
    for (size_t i=0;i<a.size();++i) if (inl[i]) {
        dx.push_back(b[i].x - a[i].x);
        dy.push_back(b[i].y - a[i].y);
    }
    if (dx.empty()) return cv::Point2f(0,0);
    auto kth = [](std::vector<float>& v){ std::nth_element(v.begin(), v.begin()+v.size()/2, v.end()); return v[v.size()/2]; };
    float mdx = kth(dx), mdy = kth(dy);
    return {mdx, mdy};
}

// median forward-backward error
static double medianFB(const std::vector<cv::Point2f>& p,
                       const std::vector<cv::Point2f>& pfb,
                       const std::vector<uchar>& inl)
{
    std::vector<float> e; e.reserve(p.size());
    for (size_t i=0;i<p.size();++i) if (inl[i]) {
        float ex = pfb[i].x - p[i].x, ey = pfb[i].y - p[i].y;
        e.push_back(std::sqrt(ex*ex+ey*ey));
    }
    if (e.empty()) return 1e9;
    std::nth_element(e.begin(), e.begin()+e.size()/2, e.end());
    return e[e.size()/2];
}

// estimating the change in scale from pairs of LK points (median of d1/d0)
static double estimateScale(const std::vector<cv::Point2f>& p0,
                            const std::vector<cv::Point2f>& p1,
                            const std::vector<uchar>& inl)
{
    std::vector<double> r; r.reserve(p0.size()*2);
    for (size_t i=0;i<p0.size();++i) if (inl[i]) {
        for (size_t j=i+1;j<p0.size();++j) if (inl[j]) {
            double d0 = cv::norm(p0[i]-p0[j]);
            double d1 = cv::norm(p1[i]-p1[j]);
            if (d0 > 1e-3 && d1 > 1e-3) r.push_back(d1/d0);
        }
    }
    if (r.empty()) return 1.0;
    std::nth_element(r.begin(), r.begin()+r.size()/2, r.end());
    return r[r.size()/2];
}

static inline bool detectNoseInFaceROI(const cv::Mat& grayFull,
                                       const cv::Rect& faceFull,
                                       cv::CascadeClassifier& noseCascade,
                                       cv::Rect& noseOut)
{
    noseOut = cv::Rect();
    if (grayFull.empty() || faceFull.width<=0 || faceFull.height<=0) return false;

    // Nose search on face: ~0.40..0.80 height
    const int nx = faceFull.x;
    const int ny = faceFull.y + int(std::round(0.40 * faceFull.height));
    const int nw = faceFull.width;
    const int nh = std::max(1, int(std::round(0.40 * faceFull.height)));
    cv::Rect search(nx, ny, nw, nh);
    search &= cv::Rect(0,0,grayFull.cols, grayFull.rows);
    if (search.width < 24 || search.height < 16) return false;

    // EQ + resize
    cv::Mat roi = grayFull(search).clone(), eq, up;
    cv::equalizeHist(roi, eq);
    cv::resize(eq, up, cv::Size(), 1.2, 1.2, cv::INTER_CUBIC);

    std::vector<cv::Rect> noses;
    // Little amount of FP
    const double scaleFactor  = 1.15;
    const int    minNeighbors = 3;
    const int    minSide      = std::min(up.cols, up.rows);
    const int    minW         = std::max(14, minSide/10);
    noseCascade.detectMultiScale(up, noses, scaleFactor, minNeighbors, 0, cv::Size(minW, minW));

    if (noses.empty()) return false;

    // Biggest candidate
    const cv::Rect best = *std::max_element(
        noses.begin(), noses.end(),
        [](const cv::Rect& a, const cv::Rect& b){ return a.area() < b.area(); });

    // Coordinates on full size pic
    cv::Rect bestInSearch(
        search.x + int(std::round(best.x / 1.2)),
        search.y + int(std::round(best.y / 1.2)),
        int(std::round(best.width  / 1.2)),
        int(std::round(best.height / 1.2))
    );
    bestInSearch &= cv::Rect(0,0,grayFull.cols, grayFull.rows);

    
    // SIMPLE GEOMETRY (cut off obvious FP):
    // - horizontal: center of nose at the middle 25..75% of face width
    // - vertical: 45..80% of height from the top of the face
    // - size: 8..35% of face height
    const double fx = double(bestInSearch.x + bestInSearch.width/2 - faceFull.x) / std::max(1, faceFull.width);
    const double fy = double(bestInSearch.y + bestInSearch.height/2 - faceFull.y) / std::max(1, faceFull.height);
    const double fh = double(bestInSearch.height) / std::max(1, faceFull.height);

    const bool centerOK = (fx >= 0.25 && fx <= 0.75);
    const bool yOK      = (fy >= 0.45 && fy <= 0.80);
    const bool sizeOK   = (fh >= 0.08 && fh <= 0.35);

    if (!(centerOK && yOK && sizeOK)) return false;

    noseOut = bestInSearch;
    return true;
}

// SMILE HELPERS
static inline cv::Rect mouthBandForFace(const cv::Rect& face, int imgW, int imgH)
{
    // Lower face strip (~45% height, 80% width) - this is usually where the mouth/teeth are
    int x = face.x + (int)std::round(0.10 * face.width);
    int y = face.y + (int)std::round(0.55 * face.height);
    int w = (int)std::round(0.80 * face.width);
    int h = (int)std::round(0.40 * face.height);
    cv::Rect r(x,y,w,h);
    r &= cv::Rect(0,0,imgW,imgH);
    return r;
}

// Returns: true if a smile was detected; fills the smileBox (coordinates in the full image) and confidence [0..1]
static bool detectSmileInFaceROI(const cv::Mat& grayFull,
                                 const cv::Rect& faceFull,
                                 cv::CascadeClassifier& smileCascade,
                                 cv::Rect& smileBox,
                                 double& confidence)
{
    smileBox = cv::Rect(); confidence = 0.0;
    if (grayFull.empty() || faceFull.width<=0 || faceFull.height<=0) return false;

    const cv::Rect band = mouthBandForFace(faceFull, grayFull.cols, grayFull.rows);
    if (band.width < 24 || band.height < 16) return false;

    // 1) Lower face ROI + light EQ and magnification (for Haar)
    cv::Mat roi = grayFull(band), eq, up;
    cv::Ptr<cv::CLAHE> clahe = cv::createCLAHE(1.8, cv::Size(8,8));
    clahe->apply(roi, eq);
    cv::resize(eq, up, cv::Size(), 1.25, 1.25, cv::INTER_CUBIC);

    // 2) Smile detection (conservative enough to limit FP)
    std::vector<cv::Rect> smiles;
    const double scaleFactor  = 1.2;   // 1.1-1.4
    const int    minNeighbors = 18;    // 18-28 (bigger -> less FP)
    const int    minSide      = std::min(up.cols, up.rows);
    const int    minW         = std::max(20, minSide/6);
    smileCascade.detectMultiScale(up, smiles, scaleFactor, minNeighbors, 0, cv::Size(minW, minW/6));

    if (smiles.empty()) return false;

    // 3) Best candidate
    const cv::Rect best = *std::max_element(smiles.begin(), smiles.end(),
                        [](const cv::Rect& a, const cv::Rect& b){ return a.area() < b.area(); });

    // 4) Fallback to full pic
    cv::Rect bestInBand(
        band.x + (int)std::round(best.x / 1.25),
        band.y + (int)std::round(best.y / 1.25),
        (int)std::round(best.width  / 1.25),
        (int)std::round(best.height / 1.25)
    );
    bestInBand &= cv::Rect(0,0,grayFull.cols, grayFull.rows);

    
    // 5) Confidence - waist size + relative width
    const double areaNorm = double(bestInBand.area()) / std::max(1, band.area());
    const double widthRel = double(bestInBand.width)  / std::max(1, band.width);
    confidence = std::clamp(0.6*areaNorm + 0.4*widthRel, 0.0, 1.0);

    smileBox = bestInBand;
    return true;
}
} // namespace

CvWorker::CvWorker(QObject* parent)
    : QObject(parent)
{
    try
    {
        m_cascade_loaded = loadCascadeFromResource(m_face_cascade_name, m_face_cascade) &&
                           loadCascadeFromResource(m_smile_cascade_name, m_smile_cascade) &&
                           loadCascadeFromResource(m_nose_cascade_name, m_nose_cascade);

        if (!m_cascade_loaded)
        {
            std::cout << "CvWorker: cascade load failed" << std::endl;
        }
    } 
    catch (const cv::Exception& e)
    {
        m_cascade_loaded = false;
        std::cout << "CvWorker: exception loading cascades " << e.what() << std::endl;
    }
    m_detThrottle.start();
    m_fullScanCooldown.start();
    m_lastConfirm.start();
}

void CvWorker::process(const QImage & src, bool forceFull)
{
    processImpl(src, /*roiHint*/nullptr, forceFull);
}

void CvWorker::process(const QImage & src, const QRect & roiHint, bool forceFull)
{
    const QRect* hint = roiHint.isNull() ? nullptr : & roiHint;
    processImpl(src, hint, forceFull);
}

void CvWorker::processImpl(const QImage & src, const QRect * roiHint, bool forceFull)
{
    if (!m_cascade_loaded)
    { 
        emit result(false, QRect());
        emit resultWithSmileConf(false, QRect(), 0.0);
        return;
    }

    if (src.isNull() || src.width() <= 0 || src.height() <= 0)
    {
        emit result(false, QRect());
        emit resultWithSmileConf(false, QRect(), 0.0);
        return;
    }

    if (src.width() < 64 || src.height() < 64)
    {
        emit result(!m_lastFace.isNull(), m_lastFace);
        emit resultWithSmileConf(!m_lastFace.isNull(), m_lastFace, 0.0);
        return;
    }

    // to gray conversion
    cv::Mat grayFull;
    switch (src.format())
    {
        case QImage::Format_Grayscale8:
        case QImage::Format_Indexed8:
            grayFull = cv::Mat(src.height(), src.width(), CV_8UC1,
                const_cast<uchar*>(src.bits()), src.bytesPerLine());
            break;
        case QImage::Format_RGB888:
        {
            cv::Mat rgb(src.height(), src.width(), CV_8UC3,
                        const_cast<uchar*>(src.bits()), src.bytesPerLine());
            cv::cvtColor(rgb, grayFull, cv::COLOR_RGB2GRAY);
            break;
        }
        case QImage::Format_ARGB32:
        case QImage::Format_ARGB32_Premultiplied:
        case QImage::Format_RGB32:
        {
            cv::Mat bgra(src.height(), src.width(), CV_8UC4,
                        const_cast<uchar*>(src.bits()), src.bytesPerLine());
            cv::cvtColor(bgra, grayFull, cv::COLOR_BGRA2GRAY);
            break;
        }
        default:
        {
            QImage tmp = src.convertToFormat(QImage::Format_RGB888);
            cv::Mat rgb(tmp.height(), tmp.width(), CV_8UC3,
                        const_cast<uchar*>(tmp.bits()), tmp.bytesPerLine());
            cv::cvtColor(rgb, grayFull, cv::COLOR_RGB2GRAY);
            break;
        }
    }

    // throttling
    if (!forceFull && roiHint == nullptr && !m_detThrottle.hasExpired(m_curIntervalMs))
    {
        const bool fresh = (!m_lastFace.isNull() && m_lastConfirm.elapsed() <= m_holdMs);
        if (fresh)
        { 
            double smileConf = 0.0;
            cv::Rect smileBox;
            bool smiling = detectSmileInFaceROI(grayFull, toCv(m_lastFace), m_smile_cascade, smileBox, smileConf);

            emit result(true, m_lastFace);
            emit resultWithSmileConf(true, m_lastFace, smiling ? smileConf : 0.0);
            return;
        }
    }

    // motion gating
    if (m_lastFace.isNull() && roiHint == nullptr && !forceFull)
    {
        cv::Mat tiny;
        cv::resize(grayFull, tiny, cv::Size(m_tinyW, m_tinyH), 0, 0, cv::INTER_AREA);
        double motion = 255.0;
        if (!m_prevTiny.empty())
        {
            cv::Mat diff; cv::absdiff(tiny, m_prevTiny, diff);
            motion = cv::mean(diff)[0]; // 0..255
        }
        m_prevTiny = tiny;

        if (motion < m_motionThresh)
        {
            // if cooldown expired try detection on full frame
            if (!m_fullScanCooldown.hasExpired(m_fullScanCooldownMs))
            {
                m_curIntervalMs = std::min(int(m_curIntervalMs * 1.5), m_maxIntervalMs);
                m_missStreak = std::min(m_missStreak + 1, 1000);
                m_detThrottle.restart();
                emit result(false, QRect());
                emit resultWithSmileConf(false, QRect(), 0.0);
                return;
            }
        }
    }

    // prepare target image
    const int W = src.width();
    const int H = src.height();
    const int baseW   = m_lastFace.isNull() ? std::min(300, m_detectTargetW) : m_detectTargetW;
    const int targetW = std::max(64, baseW);
    const double scale = (W > targetW) ? double(targetW)/double(W) : 1.0;

    cv::Mat graySmall;
    if (scale < 1.0) cv::resize(grayFull, graySmall, cv::Size(), scale, scale, cv::INTER_AREA);
    else graySmall = grayFull;

    cv::Mat detImg = graySmall;
    cv::Rect roiSmall(0, 0, graySmall.cols, graySmall.rows);
    bool usedRoi = false;

    // Priority: outer ROI hint > inner ROI
    if (roiHint && !roiHint->isNull())
    {
        QRect roiFull = growRect(clampTo(*roiHint, W, H), m_roiGrow, W, H);
        cv::Rect roiScaled(
            int(std::round(roiFull.x()      * scale)),
            int(std::round(roiFull.y()      * scale)),
            int(std::round(roiFull.width()  * scale)),
            int(std::round(roiFull.height() * scale))
        );
        roiScaled &= cv::Rect(0, 0, graySmall.cols, graySmall.rows);
        if (roiScaled.width > 0 && roiScaled.height > 0)
        {
            detImg = graySmall(roiScaled);
            roiSmall = roiScaled;
            usedRoi = true;
        }
    }
    else if (m_useRoi && !m_lastFace.isNull())
    {
        QRect roiFull = growRect(m_lastFace, m_roiGrow, W, H);
        cv::Rect roiScaled(
            int(std::round(roiFull.x()      * scale)),
            int(std::round(roiFull.y()      * scale)),
            int(std::round(roiFull.width()  * scale)),
            int(std::round(roiFull.height() * scale))
        );
        roiScaled &= cv::Rect(0, 0, graySmall.cols, graySmall.rows);
        if (roiScaled.width > 0 && roiScaled.height > 0)
        {
            detImg = graySmall(roiScaled);
            roiSmall = roiScaled;
            usedRoi = true;
        }
    }

    auto runDetect = [&](const cv::Mat & img,
                     const cv::Rect & base,
                     double scaleFactor,
                     int minNeighbors,
                     double imgScale) -> QRect
    {
        const int minSide = std::min(img.cols, img.rows);
        const int minS = std::max(24, minSide / 10);
        std::vector<cv::Rect> faces;
        m_face_cascade.detectMultiScale(img, faces, scaleFactor, minNeighbors, 0, cv::Size(minS, minS));

        if (faces.empty()) return QRect();

        const cv::Rect best = *std::max_element(
            faces.begin(), faces.end(),
            [](const cv::Rect & a, const cv::Rect & b){ return a.area() < b.area(); }
        );

        cv::Rect bestSmall = best + base.tl(); // coords in img space shifted by base
        // NOTE: usage of the imgScale passed in the argument, not the global scale
        cv::Rect full(
            int(std::round(bestSmall.x / imgScale)),
            int(std::round(bestSmall.y / imgScale)),
            int(std::round(bestSmall.width  / imgScale)),
            int(std::round(bestSmall.height / imgScale))
        );
        full &= cv::Rect(0, 0, W, H);
        return fromCv(full);
    };

    // simple test if the lower part of the face look like the lower part of the mouth
    auto mouthLikely = [&](const QRect& face)->bool
    {
        if (face.isNull()) return true; // don't block too aggressively
        cv::Rect f = toCv(face) & cv::Rect(0,0,grayFull.cols,grayFull.rows);
        if (f.width <= 0 || f.height <= 0) return true;
        cv::Rect band = mouthBandForFace(f, grayFull.cols, grayFull.rows);
        if (band.width <= 0 || band.height <= 0) return true;

        // upper face band ~35% for brightness reference, lower band = band
        cv::Rect top(f.x, f.y, f.width, std::max(1, int(std::round(0.35 * f.height))));
        cv::Scalar mt = cv::mean(grayFull(top));
        cv::Scalar mb = cv::mean(grayFull(band));
        // bottom usually darker (lips/shadow) - small delta to not trigger non-stop
        return (mb[0] < mt[0] - 5.0);
    };

    // "anti-shrink" - EMA size + clamp to [0.75..1.25]*EMA
    auto antiShrinkClamp = [&](QRect& face){
        static double wEma = 0.0, hEma = 0.0;
        const double alpha = 0.10;
        if (!m_sizeEmaInit || wEma == 0.0 || hEma == 0.0) {
            wEma = face.width(); hEma = face.height();
            m_sizeEmaInit = true;
        } else {
            wEma = (1.0 - alpha) * wEma + alpha * face.width();
            hEma = (1.0 - alpha) * hEma + alpha * face.height();
        }
        int w = std::clamp(face.width(),  int(std::round(0.75*wEma)), int(std::round(1.25*wEma)));
        int h = std::clamp(face.height(), int(std::round(0.75*hEma)), int(std::round(1.25*hEma)));
        face.setSize(QSize(w,h));
        face = clampTo(face, grayFull.cols, grayFull.rows);
    };

    // snap to Haar face in window around current frame (with re-init KLT)
    auto snapInWindow = [&](const QRect& around, double growK, int minNeighbors)->QRect
    {
        QRect winQ = growRect(around, growK, grayFull.cols, grayFull.rows);
        cv::Rect win = toCv(winQ);
        if (win.width < 40 || win.height < 40) return QRect();

        cv::Mat roi = grayFull(win).clone(), eq;
        cv::equalizeHist(roi, eq);

        // use runDetect, but with base = window
        QRect foundLocal = runDetect(eq, win, /*scaleFactor*/1.10, /*minNeighbors*/3, /*imgScale*/1.0);
        return foundLocal; // can return NULL
    };

    // cooldown for snaps + periodic counter
    static QElapsedTimer s_snapCooldown;
    static bool s_snapInit = false;
    if (!s_snapInit) { s_snapCooldown.start(); s_snapInit = true; }
    static int s_periodic = 0;

    // Step 1: Tracking if enabled
    if (m_enableTracking && m_tracking && !forceFull)
    {
        if (!m_prevGray.empty() && !m_ptsPrev.empty())
        {
            // forward LK
            std::vector<uchar> stF, stB;
            std::vector<float> errF, errB;
            m_ptsCurr.clear();
            cv::calcOpticalFlowPyrLK(m_prevGray, grayFull, m_ptsPrev, m_ptsCurr, stF, errF,
                                    cv::Size(21,21), 3,
                                    cv::TermCriteria(cv::TermCriteria::COUNT|cv::TermCriteria::EPS, 30, 0.01),
                                    0, 1e-4);
            // backward LK (FB check)
            std::vector<cv::Point2f> ptsBack(m_ptsPrev.size());
            cv::calcOpticalFlowPyrLK(grayFull, m_prevGray, m_ptsCurr, ptsBack, stB, errB,
                                    cv::Size(21,21), 3,
                                    cv::TermCriteria(cv::TermCriteria::COUNT|cv::TermCriteria::EPS, 30, 0.01),
                                    0, 1e-4);

            // inliers: both statuses OK and FB-error is small
            std::vector<uchar> inlier(m_ptsPrev.size(), 0);
            size_t inCnt=0;
            for (size_t i=0; i<m_ptsPrev.size(); ++i)
            {
                if (stF[i] && stB[i])
                {
                    float dx = ptsBack[i].x - m_ptsPrev[i].x;
                    float dy = ptsBack[i].y - m_ptsPrev[i].y;
                    float fb = std::sqrt(dx*dx+dy*dy);
                    if (fb <= m_fbMaxErr)
                    {
                        inlier[i]=1;
                        ++inCnt;
                    }
                }
            }
            const double frac = (m_ptsPrev.empty()?0.0: double(inCnt)/double(m_ptsPrev.size()));
            const double fbMed = medianFB(m_ptsPrev, ptsBack, inlier);

            if (frac >= m_inlierFracThresh && fbMed <= m_fbMaxErr)
            {
                // accept and shift bbox with median of vectors
                cv::Point2f d = medianFlow(m_ptsPrev, m_ptsCurr, inlier);
                QRect nr(m_trackRect.x() + static_cast<int>(std::round(d.x)),
                        m_trackRect.y() + static_cast<int>(std::round(d.y)),
                        m_trackRect.width(), m_trackRect.height());
                nr = clampTo(nr, W, H);

                // quality guards
                double scaleChange = estimateScale(m_ptsPrev, m_ptsCurr, inlier);
                bool goodInliers   = (frac >= m_inlierFracThresh);
                bool goodFB        = (fbMed <= m_fbMaxErr);
                bool goodScale     = (scaleChange >= 0.75 && scaleChange <= 1.35);
                bool goodMouth     = mouthLikely(nr);

                // periodic health-check every ~45 frames
                bool periodicSnap = (++s_periodic >= 45);
                if (periodicSnap) s_periodic = 0;

                // is snap needed
                bool needSnap = (!goodInliers) || (!goodFB) || (!goodScale) || (!goodMouth);

                // NOSE CHECK - cheap validator, only if the frame looks less confident or periodically
                bool allowFaceSnap = false;
                if (needSnap || periodicSnap)
                {
                    cv::Rect noseBox;
                    bool noseOK = detectNoseInFaceROI(grayFull, toCv(nr), m_nose_cascade, noseBox);
                    if (noseOK) {
                        m_noseMissStreak = 0;
                        allowFaceSnap = false; // the nose is -> don't do a heavy snap in this frame
                    } else {
                        m_noseMissStreak++;
                        // Allow face-snap only after several consecutive nose failures
                        allowFaceSnap = (m_noseMissStreak >= m_kNoseMissToSnap);
                    }
                } else {
                    // nothing was checked -> don't allow hard snap
                    allowFaceSnap = false;
                }

                // if necessary - try a local snap (and no more often than every 400 ms)
                QRect snapped;
                if (allowFaceSnap && s_snapCooldown.elapsed() >= 400) {
                    snapped = snapInWindow(nr, /*growK=*/1.6, /*minNeighbors=*/3);
                    s_snapCooldown.restart();
                }

                if (!snapped.isNull())
                {
                    m_sizeEmaInit = false; // initialize EMA on new frame
                    m_noseMissStreak = 0; 
                    // take over a new frame and initialize KLT in it (as after a fresh detection)
                    m_lastFace = snapped;
                    m_trackRect = m_lastFace;
                    m_faceVisible = true;
                    m_trackMisses = 0;
                    ++m_hitStreak;
                    m_curIntervalMs = m_detIntervalMs;

                    const cv::Rect rc = toCv(m_lastFace) & cv::Rect(0,0,grayFull.cols,grayFull.rows);
                    if (rc.width > 0 && rc.height > 0) {
                        cv::Mat mask = rectMask(grayFull.cols, grayFull.rows, m_lastFace, 0.10);
                        m_ptsPrev.clear();
                        cv::goodFeaturesToTrack(grayFull, m_ptsPrev, m_kltMaxPts, m_kltQuality, m_kltMinDist, mask);
                        m_prevGray = grayFull.clone();
                        m_tracking = m_ptsPrev.size() >= 12;
                        m_trackMisses = 0;
                    }
                } else {
                    // no snap - update based on tracking
                    m_trackRect = nr;
                    m_lastFace  = nr;
                    m_faceVisible = true;
                    m_trackMisses = 0;
                    ++m_hitStreak;
                    m_curIntervalMs = m_detIntervalMs;
                }

                // anti-shrink (size stabilization)
                antiShrinkClamp(m_lastFace);

                // SMILE (Haar in the mouth area)
                double smileConf = 0.0;
                cv::Rect smileBox;
                bool smiling = detectSmileInFaceROI(grayFull, toCv(m_lastFace), m_smile_cascade, smileBox, smileConf);

                // EMIT
                emit result(true, m_lastFace);
                emit resultWithSmileConf(true, m_lastFace, smiling ? smileConf : 0.0);

                // prepare next iteration
                m_prevGray = grayFull.clone();
                m_ptsPrev.clear();
                for (size_t i=0;i<m_ptsCurr.size();++i) if (inlier[i]) m_ptsPrev.push_back(m_ptsCurr[i]);
                return;
            }
            else
            {
                // if tracking weakens go towards detection
                m_trackMisses++;
                if (m_trackMisses >= m_trackMaxMisses)
                {
                    m_tracking = false;
                    m_ptsPrev.clear();
                }
            }
        }
    }

    // Step 2: detection in the selected detImg (ROI or full scene), with EQ for Haars
    cv::Mat detEq;
    cv::equalizeHist(detImg, detEq);
    QRect found = runDetect(detEq, roiSmall, /*scaleFactor*/1.10, /*minNeighbors*/3, /*imgScale*/scale);

    // Step 3: if there is nothing in the ROI try the full scene
    if (found.isNull() && usedRoi)
    {
        cv::Mat grayEq;
        cv::equalizeHist(graySmall, grayEq);
        found = runDetect(grayEq, cv::Rect(0,0,graySmall.cols,graySmall.rows),
                  /*scaleFactor*/1.20, /*minNeighbors*/4, /*imgScale*/scale);
        m_fullScanCooldown.restart();
    }

    // after detection try, restart cooldown
    if (m_lastFace.isNull())
    {
        m_fullScanCooldown.restart();
    }

    m_detThrottle.restart();

    if (!found.isNull())
    {
        m_lastConfirm.restart();
        m_lastFace = found;
        antiShrinkClamp(m_lastFace);
        m_missStreak = 0;
        ++m_hitStreak;

        if (!m_faceVisible && m_hitStreak < m_hitsToConfirm)
        {
            m_curIntervalMs = m_detIntervalMs;
            emit result(false, QRect());
            emit resultWithSmileConf(false, QRect(), 0.0);
            return;
        }

        m_faceVisible = true;
        m_curIntervalMs = m_detIntervalMs;
        // (Re)initialization of KLT after detection
        if (m_enableTracking)
        {
            const cv::Rect rc = toCv(m_lastFace) & cv::Rect(0,0,grayFull.cols,grayFull.rows);
            if (rc.width > 0 && rc.height > 0)
            {
                m_trackRect = m_lastFace;
                // select points only inside the face with margin
                cv::Mat mask = rectMask(grayFull.cols, grayFull.rows, m_lastFace, 0.10);
                m_ptsPrev.clear();
                cv::goodFeaturesToTrack(grayFull, m_ptsPrev, m_kltMaxPts, m_kltQuality, m_kltMinDist, mask);
                m_prevGray = grayFull.clone();
                m_tracking = m_ptsPrev.size() >= 12; // min number of points
                m_trackMisses = 0;
            }
        }

        double smileConf = 0.0;
        cv::Rect smileBox;
        bool smiling = detectSmileInFaceROI(grayFull, toCv(found), m_smile_cascade, smileBox, smileConf);

        emit result(true, found);
        emit resultWithSmileConf(true, found, smiling ? smileConf : 0.0);
    }
    else
    {
        m_hitStreak = 0;
        m_missStreak = std::min(m_missStreak + 1, 1000);
        m_curIntervalMs = std::min(int(m_curIntervalMs * 1.5), m_maxIntervalMs);

        /* If an ROI (external or from m_lastFace) was used in this
           frame don't emit the old m_lastFace, return QRect() instead. */
        if ((roiHint != nullptr) || usedRoi)
        {
            m_faceVisible = false;
            emit result(false, QRect());
            emit resultWithSmileConf(false, QRect(), 0.0);
        }
        else if (m_faceVisible && m_missStreak < m_missesToClear)
        {
            double smileConf = 0.0;
            cv::Rect smileBox;
            bool smiling = detectSmileInFaceROI(grayFull, toCv(m_lastFace), m_smile_cascade, smileBox, smileConf);

            emit result(true, m_lastFace);
            emit resultWithSmileConf(true, m_lastFace, smiling ? smileConf : 0.0);
        }
        else
        {
            m_faceVisible = false;
            emit result(false, QRect());
            emit resultWithSmileConf(false, QRect(), 0.0);
        }
    }
}

bool CvWorker::loadCascadeFromResource(const std::string & cascade_name, cv::CascadeClassifier & cascade_classifier)
{
    const QString tmp = QStandardPaths::writableLocation(QStandardPaths::TempLocation)
                        + "/" + QString::fromStdString(cascade_name);
    const std::string tmp_str_path = tmp.toStdString();
    const QString cascade_full_path = ":/data/" + QString::fromStdString(cascade_name);
    QFile f(cascade_full_path);
    if (!f.open(QIODevice::ReadOnly))
    {
        std::cout << "[GUI] No cascade in resources (:/data/" << cascade_name << std::endl;;
        return false;
    }
    const QByteArray xml = f.readAll();
    f.close();

    std::ofstream ofs(tmp_str_path, std::ios::binary | std::ios::trunc);
    if (!ofs)
    {
        std::cout << "[GUI] Tmp cascade write failed: " << tmp_str_path << std::endl;
        return false;
    }
    ofs.write(xml.constData(), static_cast<std::streamsize>(xml.size()));
    ofs.close();

    // sanity check
    if (!std::filesystem::exists(tmp_str_path) ||
        std::filesystem::file_size(tmp_str_path) == 0)
    {
        std::cout << "[GUI] Cascade file missing/empty:" << tmp_str_path << std::endl;
        return false;
    }

    if (!cascade_classifier.load(tmp_str_path))
    {
        std::cout << "[GUI] Cascade load failed: " << tmp_str_path << std::endl;
        return false;
    }

    return true;
}