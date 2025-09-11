#include "odin/video_handler/VideoHandler.h"
#include "odin/video_handler/DataTypes.h"
#include "InetCommData.h"

#include <cerrno>
#include <chrono>
#include <endian.h>
#include <fcntl.h>
#include <filesystem>
#include <iostream>
#include <linux/videodev2.h>
#include <regex>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

namespace fs = std::filesystem;

namespace odin
{
namespace video_handler
{

bool VideoHandler::m_run_process = true;

static inline uint64_t now_us()
{
    struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts);
    return uint64_t(ts.tv_sec) * 1000000ULL + ts.tv_nsec / 1000ULL;
}

static void v4l2_cleanup(V4L2Ctx& ctx)
{
    if (ctx.fd >= 0)
    {
        v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        ioctl(ctx.fd, VIDIOC_STREAMOFF, &type);
    }
    for (int i = 0; i < ctx.nbufs; ++i)
    {
        if (ctx.bufs[i].start)
        {
            munmap(ctx.bufs[i].start, ctx.bufs[i].len);
            ctx.bufs[i].start = nullptr;
            ctx.bufs[i].len = 0;
        }
    }
    if (ctx.fd >= 0) { close(ctx.fd); ctx.fd = -1; }
    ctx.nbufs = 0;
}

static bool v4l2_open_mjpg(const std::string& dev, int width, int height, int fps, V4L2Ctx& ctx)
{
    ctx = {};
    ctx.fd = open(dev.c_str(), O_RDWR | O_NONBLOCK);
    if (ctx.fd < 0) { perror("open v4l2"); return false; }

    v4l2_format fmt{}; fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width  = width;
    fmt.fmt.pix.height = height;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_MJPEG;
    fmt.fmt.pix.field  = V4L2_FIELD_NONE;
    if (ioctl(ctx.fd, VIDIOC_S_FMT, &fmt) < 0) { perror("S_FMT"); v4l2_cleanup(ctx); return false; }

    // FPS
    v4l2_streamparm sp{}; sp.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    sp.parm.capture.timeperframe.numerator = 1;
    sp.parm.capture.timeperframe.denominator = fps;
    if (ioctl(ctx.fd, VIDIOC_S_PARM, &sp) < 0) {}

    // MMAP buffers
    v4l2_requestbuffers req{}; req.count = 4; req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE; req.memory = V4L2_MEMORY_MMAP;
    if (ioctl(ctx.fd, VIDIOC_REQBUFS, &req) < 0 || req.count < 2) { perror("REQBUFS"); v4l2_cleanup(ctx); return false; }

    ctx.nbufs = std::min<int>(req.count, 8);
    for (int i = 0; i < ctx.nbufs; ++i)
    {
        v4l2_buffer b{}; b.type = req.type; b.memory = req.memory; b.index = (unsigned)i;
        if (ioctl(ctx.fd, VIDIOC_QUERYBUF, &b) < 0) { perror("QUERYBUF"); v4l2_cleanup(ctx); return false; }
        ctx.bufs[i].len = b.length;
        ctx.bufs[i].start = mmap(nullptr, b.length, PROT_READ|PROT_WRITE, MAP_SHARED, ctx.fd, b.m.offset);
        if (ctx.bufs[i].start == MAP_FAILED) { perror("mmap"); v4l2_cleanup(ctx); return false; }
        if (ioctl(ctx.fd, VIDIOC_QBUF, &b) < 0) { perror("QBUF"); v4l2_cleanup(ctx); return false; }
    }

    v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(ctx.fd, VIDIOC_STREAMON, &type) < 0) { perror("STREAMON"); v4l2_cleanup(ctx); return false; }

    ctx.width  = fmt.fmt.pix.width;
    ctx.height = fmt.fmt.pix.height;
    return true;
}

VideoHandler::VideoHandler() :
    m_width(VIDEO_WIDTH),
    m_height(VIDEO_HEIGHT),
    m_fps(VIDEO_FPS),
    m_codec(VIDEO_CODEC),
    m_src(VIDEO_SRC),
    m_verbose(true)
{
    m_next_retry_tp = std::chrono::steady_clock::time_point::min();
    std::cout << "[ARM] payload_per_pkt=" << MTU << " (sizeof hdr=" << sizeof(UdpMjpegHdr) << ")\n";
    std::cout << "[ARM] Sending to " << ROBOTIC_GUI_IP << ":" << VIDEO_PORT << "\n";

}

bool VideoHandler::isCaptureDevice(const std::string & dev, std::string * card_out)
{
    int fd = open(dev.c_str(), O_RDWR | O_NONBLOCK);
    if (fd < 0) return false;

    v4l2_capability cap{};
    bool ok = (ioctl(fd, VIDIOC_QUERYCAP, &cap) == 0);
    close(fd);
    if (!ok) return false;

    // Skip loopback and mem2mem encoders
    std::string card(reinterpret_cast<char*>(cap.card));
    std::string driver(reinterpret_cast<char*>(cap.driver));
    if (card_out) *card_out = card;
    if (card.find("loopback") != std::string::npos) return false;
    if (driver.find("vicodec") != std::string::npos) return false;

    const uint32_t caps = cap.capabilities | cap.device_caps;
    const bool capture =
        (caps & V4L2_CAP_VIDEO_CAPTURE) || (caps & V4L2_CAP_VIDEO_CAPTURE_MPLANE);
    const bool output  = (caps & V4L2_CAP_VIDEO_OUTPUT) ||
                         (caps & V4L2_CAP_VIDEO_OUTPUT_MPLANE);
    return capture && !output;
}

std::string VideoHandler::findFirstCamera()
{
    static const std::regex re("^video[0-9]+$");
    std::vector<std::string> candidates;
    for (auto& p : fs::directory_iterator("/dev"))
    {
        const auto name = p.path().filename().string();
        if (std::regex_match(name, re))
        {
            candidates.push_back("/dev/" + name);
        }
    }
    std::sort(candidates.begin(), candidates.end());
    for (auto& dev : candidates)
    {
        std::string card;
        if (isCaptureDevice(dev, &card))
        {
            std::cout << "[VideoHandler] Found camera: " << dev << " (" << card << ")\n";
            return dev;
        }
    }
    return "";
}

void VideoHandler::stopStream()
{
    if (!m_streaming) return;

    v4l2_cleanup(m_v4l2);
    m_v4l2 = {};

    m_udp.reset();
    m_seq = 0;

    m_streaming = false;
}

void VideoHandler::requestRun(bool on) { m_run_process = on; }
bool VideoHandler::desired() const { return m_run_process; }

bool VideoHandler::startStream(const std::string& dev)
{
    // V4L2
    if (!v4l2_open_mjpg(dev, m_width, m_height, m_fps, m_v4l2))
    {
        if (m_verbose) std::cerr << "[VideoHandler] v4l2_open_mjpg failed\n";
        return false;
    }

    // UDP
    try 
    {
        m_udp = std::make_unique<UdpHandler<uint8_t>>(MAX_PKT, VIDEO_PORT, ROBOTIC_GUI_IP);
        m_udp->set_send_buffer_bytes(4 * 1024 * 1024);
        m_udp->set_dscp(8);
    } 
    catch (...) 
    {
        v4l2_cleanup(m_v4l2);
        m_v4l2 = {};
        return false;
    }

    m_seq = 0;
    m_streaming = true;
    if (m_verbose) 
    {
        std::cout << "[VideoHandler] streaming "
                  << m_v4l2.width << "x" << m_v4l2.height
                  << "@" << m_fps << " to " << ROBOTIC_GUI_IP << ":" << VIDEO_PORT << "\n";
    }
    return true;
}

void VideoHandler::tick() {
    static int tick_no = 0;
    if ((tick_no++ % 400) == 0) 
    {
        std::cout << "[VideoHandler] tick streaming=" << m_streaming
                  << " runRequested=" << m_run_process
                  << " now<retry?=" << (std::chrono::steady_clock::now() < m_next_retry_tp)
                  << " last_dev='" << m_last_dev << "'\n";
    }

    if (!m_run_process) { stopStream(); return; }

    // If stream works, check camera
    static auto next_dev_check = std::chrono::steady_clock::now();
    auto now = std::chrono::steady_clock::now();
    if (m_streaming) 
    {
        if (now < next_dev_check) return; // don't check more frequently
        next_dev_check = now + std::chrono::seconds(2);
        std::string dev = findFirstCamera();
        if (!dev.empty() && dev != m_last_dev) {
            if (m_verbose) std::cout << "[VideoHandler] Camera changed: " << m_last_dev << " -> " << dev << "\n";
            stopStream(); m_last_dev = dev;
            m_next_retry_tp = now + std::chrono::milliseconds(200);
        }
        return;
    }

    // Is stream doesn't work just return
    if (std::chrono::steady_clock::now() < m_next_retry_tp) return;

    // Find camera
    std::string dev = findFirstCamera();
    if (dev.empty()) 
    {
        if (m_verbose) std::cout << "[VideoHandler] No V4L2 camera found. Retrying...\n";
        m_next_retry_tp = std::chrono::steady_clock::now() + std::chrono::seconds(2);
        return;
    }
    if (dev != m_last_dev)
    {
        if (m_verbose) std::cout << "[VideoHandler] Using camera: " << dev << "\n";
        m_last_dev = dev;
    }

    if (!startStream(m_last_dev))
    {
        m_next_retry_tp = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    }
}

void VideoHandler::run() 
{
    if (!m_run_process || !m_streaming) return;

    fd_set rfds; FD_ZERO(&rfds); FD_SET(m_v4l2.fd, &rfds);
    timeval tv{0, 5000}; // 5 ms
    int r = ::select(m_v4l2.fd + 1, &rfds, nullptr, nullptr, &tv);
    if (r <= 0 || !FD_ISSET(m_v4l2.fd, &rfds)) return;

    v4l2_buffer b{}; b.type = V4L2_BUF_TYPE_VIDEO_CAPTURE; b.memory = V4L2_MEMORY_MMAP;
    if (ioctl(m_v4l2.fd, VIDIOC_DQBUF, &b) < 0) 
    {
        if (errno == EAGAIN) return;
        perror("[VideoHandler] DQBUF");
        stopStream();
        m_next_retry_tp = std::chrono::steady_clock::now() + std::chrono::milliseconds(200);
        return;
    }

    const uint8_t* frame = static_cast<uint8_t*>(m_v4l2.bufs[b.index].start);
    size_t len = b.bytesused;
    const uint64_t ts = now_us();

    const size_t payload = MTU;
    uint16_t frag_cnt = (uint16_t)((len + payload - 1) / payload);
    if (frag_cnt == 0) frag_cnt = 1;

    for (uint16_t i = 0; i < frag_cnt; ++i)
    {
        size_t off   = size_t(i) * payload;
        size_t chunk = std::min(payload, len - off);

        auto* h = reinterpret_cast<UdpMjpegHdr*>(m_pkt.data());
        h->magic    = htonl(0x4D4A5047);
        h->seq      = htonl(m_seq);
        h->frag_idx = htons(i);
        h->frag_cnt = htons(frag_cnt);
        h->ts_us    = htobe64(ts);
        h->width    = htons(m_v4l2.width);
        h->height   = htons(m_v4l2.height);

        std::memcpy(m_pkt.data() + sizeof(UdpMjpegHdr), frame + off, chunk);
        (void)m_udp->write(m_pkt.data(), sizeof(UdpMjpegHdr) + chunk, /*100ms*/100000);
    }
    ++m_seq;

    if (ioctl(m_v4l2.fd, VIDIOC_QBUF, &b) < 0) 
    {
        perror("[VideoHandler] QBUF");
        stopStream();
        m_next_retry_tp = std::chrono::steady_clock::now() + std::chrono::milliseconds(200);
        return;
    }
}

void VideoHandler::signalCallbackHandler(int signum)
{
    std::cout << "VideoHandler received signal: " << signum << std::endl;
    m_run_process = false;
}

} // video_handler
} // odin