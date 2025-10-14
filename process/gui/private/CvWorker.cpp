#include "CvWorker.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <opencv2/imgproc.hpp>
#include <opencv2/video/tracking.hpp>

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
static inline double iou(const QRect& a, const QRect& b) {
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
static cv::Mat rectMask(int W, int H, const QRect& r, double marginFrac=0.1) {
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

// median forward–backward error
static double medianFB(const std::vector<cv::Point2f>& p,
                       const std::vector<cv::Point2f>& pfb,
                       const std::vector<uchar>& inl){
    std::vector<float> e; e.reserve(p.size());
    for (size_t i=0;i<p.size();++i) if (inl[i]) {
        float ex = pfb[i].x - p[i].x, ey = pfb[i].y - p[i].y;
        e.push_back(std::sqrt(ex*ex+ey*ey));
    }
    if (e.empty()) return 1e9;
    std::nth_element(e.begin(), e.begin()+e.size()/2, e.end());
    return e[e.size()/2];
}
} // namespace

CvWorker::CvWorker(const std::string& cascadePath, QObject* parent)
    : QObject(parent)
{
    try
    {
        m_cascade_loaded = m_cascade.load(cascadePath);
        if (!m_cascade_loaded)
        {
            std::cout << "CvWorker: cascade.load(" << cascadePath.c_str() << ") returned false";
        }
    } 
    catch (const cv::Exception& e)
    {
        m_cascade_loaded = false;
        std::cout << "CvWorker: exception loading cascade " << cascadePath.c_str() << " " << e.what();
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
    if (!m_cascade_loaded) { emit result(false, QRect()); return; }

    if (src.isNull() || src.width() <= 0 || src.height() <= 0) {
        emit result(false, QRect()); return;
    }

    if (src.width() < 64 || src.height() < 64) {
        emit result(!m_lastFace.isNull(), m_lastFace); return;
    }

    // throttling
    if (!forceFull && roiHint == nullptr && !m_detThrottle.hasExpired(m_curIntervalMs))
    {
        const bool fresh = (!m_lastFace.isNull() && m_lastConfirm.elapsed() <= m_holdMs);
        if (fresh) { emit result(true, m_lastFace); return; }
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
            if (!m_fullScanCooldown.hasExpired(m_fullScanCooldownMs)) {
                m_curIntervalMs = std::min(int(m_curIntervalMs * 1.5), m_maxIntervalMs);
                m_missStreak = std::min(m_missStreak + 1, 1000);
                m_detThrottle.restart();
                emit result(false, QRect());
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

    auto runDetect = [&](const cv::Mat & img, const cv::Rect & base, double scaleFactor, int minNeighbors) -> QRect
    {
        const int minSide = std::min(img.cols, img.rows);
        const int minS = std::max(24, minSide / 10);
        std::vector<cv::Rect> faces;
        m_cascade.detectMultiScale(img, faces, scaleFactor, minNeighbors, 0, cv::Size(minS, minS));

        if (faces.empty()) return QRect();

        const cv::Rect best = *std::max_element(
            faces.begin(), faces.end(),
            [](const cv::Rect & a, const cv::Rect & b){ return a.area() < b.area(); }
        );

        cv::Rect bestSmall = best + base.tl(); // shift ROI on a "small" scale
        cv::Rect full(
            int(std::round(bestSmall.x / scale)),
            int(std::round(bestSmall.y / scale)),
            int(std::round(bestSmall.width  / scale)),
            int(std::round(bestSmall.height / scale))
        );
        full &= cv::Rect(0, 0, W, H);
        return fromCv(full);
    };

    // Step 1: Trakiching if enabled
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
                m_trackRect = nr;
                m_lastFace  = nr;
                m_faceVisible = true;
                m_trackMisses = 0;
                ++m_hitStreak;
                m_curIntervalMs = m_detIntervalMs;

                emit result(true, m_lastFace);
                // emit resultWithConf(true, m_lastFace, frac);

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
    QRect found = runDetect(detEq, roiSmall, /*scaleFactor*/1.10, /*minNeighbors*/3);

    // Step 3: if there is nothing in the ROI try the full scene
    if (found.isNull() && usedRoi)
    {
        cv::Mat grayEq;
        cv::equalizeHist(graySmall, grayEq);
        found = runDetect(grayEq,
                          cv::Rect(0, 0, graySmall.cols, graySmall.rows),
                          /*scaleFactor*/1.20, /*minNeighbors*/4);
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
        m_missStreak = 0;
        ++m_hitStreak;

        if (!m_faceVisible && m_hitStreak < m_hitsToConfirm)
        {
            m_curIntervalMs = m_detIntervalMs;
            emit result(false, QRect());
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
        emit result(true, found);
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
            emit resultWithConf(false, QRect(), 0.0);
        }
        else if (m_faceVisible && m_missStreak < m_missesToClear)
        {
            emit result(true, m_lastFace);
        }
        else
        {
            m_faceVisible = false;
            emit result(false, QRect());
            emit resultWithConf(false, QRect(), 0.0);
        }
    }
}