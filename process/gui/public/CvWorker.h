#ifndef CVWORKER_H
#define CVWORKER_H

#include <string>

#include <opencv2/core.hpp>   // cv::Mat
#include <opencv2/objdetect.hpp> // cv::CascadeClassifier

#include <QElapsedTimer>
#include <QImage>
#include <QObject>
#include <QRect>

class CvWorker : public QObject {
    Q_OBJECT
public:
    explicit CvWorker(const std::string & cascadePath, QObject* parent=nullptr);
    ~CvWorker() = default;

    // Detector config parameters setters
    /// \param width value to which the image should be scaled for detection
    void setDetectTargetWidth(int width) { m_detectTargetW = width; }
    /// \param on boolean to use ROI or not
    void setUseRoi(bool on) { m_useRoi = on; }
    /// \param grow_factor value to increase the ROI size if face was not detected
    void setRoiGrow(double grow_factor) { m_roiGrow = grow_factor; } // ex. 1.6–2.0
    /// \param ms value in milliseconds for time between detections
    void setDetectIntervalMs(int ms) { m_detIntervalMs = ms; } // ex. 600–900 ms

    /// turn on/off tracking
    void enableTracking(bool on) { m_enableTracking = on; }
    /// how many tracker failures allow before give up
    void setTrackMaxMisses(int n) { m_trackMaxMisses = n; }
public slots:
    /// \param src source image to process
    /// \param forceFull detect on full image despite ROI/throttling
    void process(const QImage & src, bool forceFull = false);
    /// \brief Process frame with an optional externally provided ROI (in FULL-res coordinates).
    /// If roiHint is non-empty, detection prefers this ROI (grown by m_roiGrow) before falling back.
    /// \param src      source image to process
    /// \param roiHint  ROI hint in full-resolution coords (QRect::isNull()==false to use)
    /// \param forceFull detect on full image despite throttling (full-scan fallback still respects cooldown)
    void process(const QImage & src, const QRect & roiHint, bool forceFull = false);

signals:
    /// result signal
    /// \param found if the face was found
    /// \param rect QRect object to draw on the face area
    void result(bool found, const QRect & rect);
    /// result signal with confidence (for tracker)
    void resultWithConf(bool found, const QRect & rect, double conf);
private:
    // Internal implementation shared by both public slots.
    void processImpl(const QImage & src, const QRect * roiHint, bool forceFull);
    cv::CascadeClassifier m_cascade; // Haar face detector
    bool m_cascade_loaded = false;

    // Detector config parameters
    int m_detectTargetW = 360; // width to which the image should be scaled for detection
    bool m_useRoi       = true;
    double m_roiGrow    = 1.8;
    int m_detIntervalMs = 700; // time between detections
    int m_curIntervalMs = 700; // adaptively increased on misses
    int m_maxIntervalMs = 3000;

    QElapsedTimer m_detThrottle; // time since last detection
    QElapsedTimer m_fullScanCooldown; // spacing for full-frame fallback
    int m_fullScanCooldownMs = 1500;
    QElapsedTimer m_lastConfirm; // last confirmed face detection
    int m_holdMs = 250; // how long keep bbox without face detection

    QRect m_lastFace; // last known face position

    // Motion gating (only when we have no face): skip detect if scene is static
    cv::Mat m_prevTiny; // previous tiny grayscale frame
    int     m_tinyW = 80, m_tinyH = 60;
    double  m_motionThresh = 3.0; // mean abs diff threshold (0..255)
    int     m_missStreak = 0; // consecutive misses - grow interval

    // Appearance/Fade Hysteresis
    int  m_hitsToConfirm = 2; // K: how many hits to "turn on" face
    int  m_missesToClear = 1; // M: how many miss to "turn off" face (after hold)
    int  m_hitStreak = 0;
    bool m_faceVisible = false;

    // Tracking state (KLT/Lucas–Kanade with FB-check)
    bool   m_enableTracking = true;
    bool   m_tracking = false;
    int    m_trackMaxMisses = 5;
    int    m_trackMisses = 0;
    QRect  m_trackRect; // last bbox
    // KLT: point + last frame (gray)
    std::vector<cv::Point2f> m_ptsPrev, m_ptsCurr;
    cv::Mat m_prevGray; // last full frame (gray)
    int     m_kltMaxPts = 100;
    double  m_kltQuality = 0.01;
    double  m_kltMinDist = 5.0;
    double  m_fbMaxErr = 1.5; // forward-backward max median error (px)
    double  m_inlierFracThresh = 0.6; // min inlier faction
};

#endif // CVWORKER_H