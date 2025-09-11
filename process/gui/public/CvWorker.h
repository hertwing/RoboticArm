#ifndef CVWORKER_H
#define CVWORKER_H

#include <string>
#include <QObject>
#include <QImage>
#include <QElapsedTimer>
#include <opencv2/objdetect.hpp>
#include <opencv2/imgproc.hpp>

class CvWorker : public QObject {
    Q_OBJECT
public:
    explicit CvWorker(const std::string & cascadePath, QObject* parent=nullptr);
public slots:
    void process(const QImage& src, bool forceFull);
signals:
    void result(bool found, const QRect& rect);

private:
    cv::CascadeClassifier m_cascade;
    bool m_ok=false;
    QElapsedTimer m_timer;
    QRect m_lastFace;
};

#endif // CVWORKER_H