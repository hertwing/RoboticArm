#include "v4l2capture.h"
#include <QThread>
#include <QByteArray>
#include <QImage>
#include <QBuffer>

#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/videodev2.h>
#include <string.h>
#include <sys/select.h>

static int xioctl(int fd, unsigned long request, void* arg)
{
    int r;
    do
    { 
        r = ioctl(fd, request, arg);
    }
    while (r == -1 && errno == EINTR);
    return r;
}

V4L2Capture::V4L2Capture(const QString& device,
                         int width,
                         int height,
                         int fps,
                         QObject* parent) :
    QObject(parent),
    m_devicePath(device),
    m_width(width),
    m_height(height),
    m_fps(fps)
{}

V4L2Capture::~V4L2Capture()
{
    stop();
    cleanup();
}

void V4L2Capture::start()
{
    if (m_running.load()) return;

    if (!openDevice())
    { 
        emit fatalError("Failed to open device"); 
        return; 
    }
    if (!initDevice())
    { 
        emit fatalError("Failed to init device"); 
        return; 
    }
    if (!startStream())
    { 
        emit fatalError("Failed to start stream");
        return; 
    }

    m_running = true;

    QThread* worker = QThread::create([this]{ captureLoop(); });
    connect(worker, &QThread::finished, worker, &QObject::deleteLater);
    worker->start();
}

void V4L2Capture::stop()
{
    m_running = false;
}

bool V4L2Capture::openDevice()
{
    m_fd = ::open(m_devicePath.toStdString().c_str(), O_RDWR | O_NONBLOCK, 0);
    return m_fd >= 0;
}

bool V4L2Capture::initDevice()
{
    v4l2_capability cap{};
    if (xioctl(m_fd, VIDIOC_QUERYCAP, &cap) == -1) return false;
    if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) return false;
    if (!(cap.capabilities & V4L2_CAP_STREAMING)) return false;

    v4l2_format fmt{};
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width  = m_width;
    fmt.fmt.pix.height = m_height;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_MJPEG;
    fmt.fmt.pix.field  = V4L2_FIELD_ANY;

    if (xioctl(m_fd, VIDIOC_S_FMT, &fmt) == -1) 
    {
        return false;
    }

    if (fmt.fmt.pix.pixelformat != V4L2_PIX_FMT_MJPEG) 
    {
        fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
        if (xioctl(m_fd, VIDIOC_S_FMT, &fmt) == -1) return false;
    }
    m_pixfmt = fmt.fmt.pix.pixelformat;
    m_width  = fmt.fmt.pix.width;
    m_height = fmt.fmt.pix.height;

    v4l2_streamparm parm{};
    parm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    parm.parm.capture.timeperframe.numerator   = 1;
    parm.parm.capture.timeperframe.denominator = m_fps;
    xioctl(m_fd, VIDIOC_S_PARM, &parm);

    v4l2_requestbuffers req{};
    req.count = 4;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;
    if (xioctl(m_fd, VIDIOC_REQBUFS, &req) == -1) return false;
    if (req.count < 2) return false;

    m_buffers.resize(req.count);
    for (unsigned i=0; i<req.count; ++i) 
    {
        v4l2_buffer buf{};
        buf.type = req.type;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;
        if (xioctl(m_fd, VIDIOC_QUERYBUF, &buf) == -1) return false;
        m_buffers[i].length = buf.length;
        m_buffers[i].start = mmap(NULL, buf.length, PROT_READ|PROT_WRITE, MAP_SHARED, m_fd, buf.m.offset);
        if (m_buffers[i].start == MAP_FAILED) return false;
    }

    for (unsigned i=0; i<m_buffers.size(); ++i) 
    {
        v4l2_buffer buf{};
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;
        if (xioctl(m_fd, VIDIOC_QBUF, &buf) == -1) return false;
    }

    return true;
}

bool V4L2Capture::startStream()
{
    v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    return xioctl(m_fd, VIDIOC_STREAMON, &type) != -1;
}

void V4L2Capture::captureLoop()
{
    fd_set fds;
    timeval tv{};
    while (m_running.load()) 
    {
        FD_ZERO(&fds);
        FD_SET(m_fd, &fds);
        tv.tv_sec = 2;
        tv.tv_usec = 0;

        int r = select(m_fd+1, &fds, nullptr, nullptr, &tv);
        if (r == -1) 
        {
            if (errno == EINTR) continue;
            emit fatalError("select() failed");
            break;
        }
        if (r == 0) 
        {
            // timeout - no frame
            continue;
        }

        v4l2_buffer buf{};
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;

        if (xioctl(m_fd, VIDIOC_DQBUF, &buf) == -1) 
        {
            if (errno == EAGAIN) continue;
            emit fatalError("DQBUF failed");
            break;
        }

        const void* data = m_buffers[buf.index].start;
        size_t len = buf.bytesused;

        QImage img = makeQImageFromBuffer(data, len);
        if (!img.isNull()) 
        {
            emit frameReady(img);
        }

        if (xioctl(m_fd, VIDIOC_QBUF, &buf) == -1)
        {
            emit fatalError("QBUF failed");
            break;
        }
    }

    cleanup();
}

void V4L2Capture::cleanup()
{
    if (m_fd >= 0) {
        v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        xioctl(m_fd, VIDIOC_STREAMOFF, &type);
    }
    for (auto& b : m_buffers) {
        if (b.start && b.start != MAP_FAILED && b.length)
            munmap(b.start, b.length);
    }
    m_buffers.clear();
    if (m_fd >= 0)
    { 
        ::close(m_fd); m_fd = -1;
    }
}

QImage V4L2Capture::makeQImageFromBuffer(const void* data, size_t len)
{
    if (m_pixfmt == V4L2_PIX_FMT_MJPEG)
    {
        QByteArray ba(reinterpret_cast<const char*>(data), static_cast<int>(len));
        QImage img = QImage::fromData(ba, "JPG");
        if (img.format() != QImage::Format_RGB888)
            img = img.convertToFormat(QImage::Format_RGB888);
        return img;
    } 
    else if (m_pixfmt == V4L2_PIX_FMT_YUYV) 
    {
        return yuyvToQImage(data);
    }
    return {};
}

QImage V4L2Capture::yuyvToQImage(const void* data)
{
    const uint8_t* yuyv = static_cast<const uint8_t*>(data);
    QImage img(m_width, m_height, QImage::Format_RGB888);
    uint8_t* rgb = img.bits();

    for (int y = 0; y < m_height; ++y) {
        const uint8_t* row = yuyv + y * m_width * 2;
        uint8_t* out = rgb + y * m_width * 3;
        for (int x = 0; x < m_width; x += 2) {
            int Y0 = row[0];
            int U  = row[1];
            int Y1 = row[2];
            int V  = row[3];
            row += 4;

            auto toRGB = [](int Y, int U, int V, uint8_t* dst){
                int C = Y - 16;
                int D = U - 128;
                int E = V - 128;
                int R = (298*C + 409*E + 128) >> 8;
                int G = (298*C - 100*D - 208*E + 128) >> 8;
                int B = (298*C + 516*D + 128) >> 8;
                dst[0] = static_cast<uint8_t>(std::clamp(B, 0, 255));
                dst[1] = static_cast<uint8_t>(std::clamp(G, 0, 255));
                dst[2] = static_cast<uint8_t>(std::clamp(R, 0, 255));
            };

            toRGB(Y0, U, V, out);      out += 3;
            toRGB(Y1, U, V, out);      out += 3;
        }
    }
    return img;
}