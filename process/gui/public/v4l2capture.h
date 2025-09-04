#ifndef V4L2CAPTURE_H
#define V4L2CAPTURE_H

#include <QObject>
#include <QImage>
#include <QString>
#include <atomic>
#include <vector>
#include <sys/types.h>

class V4L2Capture : public QObject
{
    Q_OBJECT
public:
    explicit V4L2Capture(const QString& device = "/dev/video0",
                         int width = 1280, int height = 720,
                         int fps = 30,
                         QObject* parent = nullptr);
    ~V4L2Capture();

public slots:
    void start();
    void stop();

signals:
    void frameReady(const QImage& img);
    void fatalError(const QString& msg);

private:
    bool openDevice();
    bool initDevice();
    bool startStream();
    void captureLoop();
    void cleanup();

    QImage makeQImageFromBuffer(const void* data, size_t len);
    QImage yuyvToQImage(const void* data);

private:
    QString m_devicePath;
    int m_fd = -1;
    int m_width = 1280;
    int m_height = 720;
    int m_fps = 30;

    uint32_t m_pixfmt = 0; // V4L2_PIX_FMT_MJPEG or V4L2_PIX_FMT_YUYV
    struct Buffer { void* start=nullptr; size_t length=0; };
    std::vector<Buffer> m_buffers;

    std::atomic<bool> m_running{false};
};

#endif