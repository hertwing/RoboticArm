#include "CvWorker.h"

#include <string>
#include <QObject>
#include <QImage>
#include <QElapsedTimer>
#include <opencv2/objdetect.hpp>
#include <opencv2/imgproc.hpp>

CvWorker::CvWorker(const std::string& cascadePath, QObject* parent)
    : QObject(parent)
{
    try
    {
        m_ok = m_cascade.load(cascadePath);
        if (!m_ok) {
            qWarning("CvWorker: cascade.load('%s') returned false", cascadePath.c_str());
        }
    }
    catch (const cv::Exception & e)
    {
        m_ok = false;
        qWarning("CvWorker: exception while loading cascade '%s': %s",
                 cascadePath.c_str(), e.what());
    }
    m_timer.start();
}

void CvWorker::process(const QImage & src, bool forceFull) {
    if (!m_ok) { emit result(false, QRect()); return; }

    // throttling: max ~10 Hz (every 100 ms)
    if (!forceFull && m_timer.elapsed() < 100) {
        emit result(true, m_lastFace); // don't calculate, add previous
        return;
    }
    m_timer.restart();

    QImage frame = (src.format() == QImage::Format_RGB888)
        ? src : src.convertToFormat(QImage::Format_RGB888);

    const int W = frame.width();
    const int H = frame.height();

    // Downscale
    const int targetW = 320;
    const double scale = (W > targetW) ? (double)targetW / (double)W : 1.0;

    cv::Mat rgb(H, W, CV_8UC3,
                const_cast<uchar*>(frame.bits()), frame.bytesPerLine());

    cv::Mat small;
    if (scale < 1.0)
        cv::resize(rgb, small, cv::Size(), scale, scale, cv::INTER_AREA);
    else
        small = rgb;

    cv::Mat gray;  cv::cvtColor(small, gray, cv::COLOR_RGB2GRAY);

    const int minSide = std::min(gray.cols, gray.rows);
    const int ms = std::max(28, minSide / 8);
    std::vector<cv::Rect> facesSmall;
    m_cascade.detectMultiScale(gray, facesSmall, 1.25, 5, 0, cv::Size(ms, ms));

    if (facesSmall.empty()) {
        emit result(false, QRect());
        return;
    }

    const auto best = *std::max_element(facesSmall.begin(), facesSmall.end(),
                    [](auto& a, auto& b){ return a.area() < b.area(); });

    // Rescale
    const double inv = 1.0 / scale;
    QRect face(int(best.x*inv), int(best.y*inv),
                int(best.width*inv), int(best.height*inv));
    m_lastFace = face;
    emit result(true, face);
}
