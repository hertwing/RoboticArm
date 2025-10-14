#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "odin/video_handler/VideoHandler.h"
#include "odin/video_handler/DataTypes.h"
#include "InetCommData.h"

#include <atomic>
#include <cerrno>
#include <chrono>
#include <endian.h>
#include <fcntl.h>
#include <filesystem>
#include <iostream>
#include <linux/videodev2.h>
#include <regex>
#include <vector>
#include <array>
#include <algorithm>
#include <cstring>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <sys/socket.h> 
#include <sys/uio.h>
#include <netinet/in.h>
#include <sched.h>

namespace fs = std::filesystem;

namespace odin
{
namespace video_handler
{

struct TxStats
{
    uint64_t frames=0, sent_ok=0, dropped=0;
    uint64_t bytes=0, minB=UINT64_MAX, maxB=0;
    uint64_t sumFrags=0, maxFrags=0;
    uint64_t deadlineMiss=0, sendErr=0, usedFallback=0;
    uint64_t sumSendUs=0, maxSendUs=0;
    uint64_t sumBatches=0, maxBatch=0;
    uint64_t sumEagain=0, maxEagain=0;

    std::chrono::steady_clock::time_point t0 = std::chrono::steady_clock::now();

    void note(uint32_t jpgB, uint32_t frags, bool ok,
              uint64_t sendUs, uint32_t batches, uint32_t eagain,
              bool missDeadline, bool anyErr, bool fallback)
    {
        ++frames;
        bytes += jpgB;
        minB = std::min<uint64_t>(minB, jpgB);
        maxB = std::max<uint64_t>(maxB, jpgB);
        sumFrags += frags;
        if (frags > maxFrags) maxFrags = frags;
        sumSendUs += sendUs;
        if (sendUs > maxSendUs) maxSendUs = sendUs;
        sumBatches += batches;
        if (batches > maxBatch) maxBatch = batches;
        sumEagain += eagain;
        if (eagain > maxEagain) maxEagain = eagain;

        if (ok) ++sent_ok; else ++dropped;
        if (missDeadline) ++deadlineMiss;
        if (anyErr) ++sendErr;
        if (fallback) ++usedFallback;

        auto now = std::chrono::steady_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - t0).count();
        if (ms >= 2000)
        {
            const double avgB = frames ? double(bytes)/frames : 0.0;
            const double avgF = frames ? double(sumFrags)/frames : 0.0;
            const double avgUs= frames ? double(sumSendUs)/frames : 0.0;
            const double avgBt= frames ? double(sumBatches)/frames : 0.0;
            const double avgEg= frames ? double(sumEagain)/frames : 0.0;
            std::cout << "[TX] window_ms=" << ms
                      << " frames=" << frames
                      << " ok=" << sent_ok
                      << " drop=" << dropped
                      << " deadline_miss=" << deadlineMiss
                      << " send_err=" << sendErr
                      << " fallback=" << usedFallback
                      << " | avg_size=" << uint32_t(avgB) << "B"
                      << " min=" << (minB==UINT64_MAX?0:minB) << "B"
                      << " max=" << maxB << "B"
                      << " avg_frags=" << avgF
                      << " max_frags=" << maxFrags
                      << " avg_send_us=" << uint32_t(avgUs)
                      << " max_send_us=" << maxSendUs
                      << " avg_batches=" << avgBt
                      << " max_batch=" << maxBatch
                      << " avg_eagain=" << avgEg
                      << " max_eagain=" << maxEagain
                      << std::endl;
            *this = TxStats(); // reset
        }
    }
};
static TxStats g_tx;

std::atomic<bool> VideoHandler::m_run_process{true};

static inline uint64_t now_us()
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
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
    if (ctx.fd >= 0)
    {
        close(ctx.fd);
        ctx.fd = -1;
    }
    ctx.nbufs = 0;
}

static bool v4l2_open_mjpg(const std::string& dev, int width, int height, int fps, V4L2Ctx& ctx)
{
    ctx = {};
    ctx.fd = open(dev.c_str(), O_RDWR | O_NONBLOCK);
    if (ctx.fd < 0)
    {
        perror("open v4l2");
        return false;
    }

    v4l2_format fmt{};
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width  = width;
    fmt.fmt.pix.height = height;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_MJPEG;
    fmt.fmt.pix.field  = V4L2_FIELD_NONE;
    if (ioctl(ctx.fd, VIDIOC_S_FMT, &fmt) < 0)
    { 
        perror("S_FMT");
        v4l2_cleanup(ctx);
        return false;
    }

    v4l2_control ctrl{};
    ctrl.id = V4L2_CID_JPEG_COMPRESSION_QUALITY;
    ctrl.value = 60;
    if (ioctl(ctx.fd, VIDIOC_S_CTRL, &ctrl) < 0)
    {
        perror("S_CTRL JPEG_QUALITY");
    }

    // FPS
    v4l2_streamparm sp{};
    sp.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    sp.parm.capture.timeperframe.numerator = 1;
    sp.parm.capture.timeperframe.denominator = fps;
    if (ioctl(ctx.fd, VIDIOC_S_PARM, &sp) < 0)
    {
        perror("S_PARM");
    }

    // MMAP buffers
    v4l2_requestbuffers req{};
    req.count = 4;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;
    if (ioctl(ctx.fd, VIDIOC_REQBUFS, &req) < 0 || req.count < 2)
    {
        perror("REQBUFS");
        v4l2_cleanup(ctx);
        return false;
    }

    ctx.nbufs = std::min<int>(req.count, 8);
    for (int i = 0; i < ctx.nbufs; ++i)
    {
        v4l2_buffer b{};
        b.type = req.type;
        b.memory = req.memory;
        b.index = static_cast<unsigned>(i);
        if (ioctl(ctx.fd, VIDIOC_QUERYBUF, &b) < 0)
        {
            perror("QUERYBUF");
            v4l2_cleanup(ctx);
            return false;
        }
        ctx.bufs[i].len = b.length;
        ctx.bufs[i].start = mmap(nullptr, b.length, PROT_READ|PROT_WRITE, MAP_SHARED, ctx.fd, b.m.offset);
        if (ctx.bufs[i].start == MAP_FAILED)
        {
            perror("mmap");
            v4l2_cleanup(ctx);
            return false;
        }
        if (ioctl(ctx.fd, VIDIOC_QBUF, &b) < 0)
        {
            perror("QBUF");
            v4l2_cleanup(ctx);
            return false;
        }
    }

    v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(ctx.fd, VIDIOC_STREAMON, &type) < 0)
    {
        perror("STREAMON");
        v4l2_cleanup(ctx);
        return false;
    }

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
    m_hdrs.reserve(64);
    m_iov.reserve(64);
    m_msgs.reserve(64);
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

void VideoHandler::requestRun(bool on)
{
    m_run_process = on;
}
bool VideoHandler::desired() const
{
    return m_run_process;
}

bool VideoHandler::startStream(const std::string& dev)
{
    // V4L2
    if (!v4l2_open_mjpg(dev, m_width, m_height, m_fps, m_v4l2))
    {
        if (m_verbose) std::cout << "[VideoHandler] v4l2_open_mjpg failed\n";
        return false;
    }
    // UDP
    try 
    {
        m_udp = std::make_unique<UdpHandler<uint8_t>>(MAX_PKT, VIDEO_PORT, ROBOTIC_GUI_IP);
        m_udp->set_send_buffer_bytes(UDP_BUFF);
        m_udp->set_dscp(VIDEO_DSCP);
        int prio = 6; // 0..6
        (void)::setsockopt(m_udp->native_fd(), SOL_SOCKET, SO_PRIORITY, &prio, sizeof(prio));

        int want = UDP_BUFF, got=0; socklen_t glen=sizeof(got);
        ::setsockopt(m_udp->native_fd(), SOL_SOCKET, SO_SNDBUF, &want, sizeof(want));
        if (::getsockopt(m_udp->native_fd(), SOL_SOCKET, SO_SNDBUF, &got, &glen) == 0)
        {
            std::cout << "[TX] SO_SNDBUF effective=" << got << " bytes\n";
        }
    } 
    catch (...) 
    {
        v4l2_cleanup(m_v4l2);
        m_v4l2 = {};
        return false;
    }

    size_t max_frame_len = 0;
    for (int i = 0; i < m_v4l2.nbufs; ++i)
        max_frame_len = std::max(max_frame_len, m_v4l2.bufs[i].len);

    uint16_t frag_cnt_max = (uint16_t)((max_frame_len + PAYLOAD_BYTES - 1) / PAYLOAD_BYTES);

    // sanity cap
    const uint16_t FRAG_HARD_CAP = 2048;
    frag_cnt_max = std::max<uint16_t>(1, std::min<uint16_t>(frag_cnt_max, FRAG_HARD_CAP));

    m_hdrs.resize(frag_cnt_max);
    m_iov.resize(frag_cnt_max);
    m_msgs.resize(frag_cnt_max);

    for (uint16_t i = 0; i < frag_cnt_max; ++i)
    {
        std::memset(&m_msgs[i], 0, sizeof(m_msgs[i]));
        m_iov[i][0].iov_base = &m_hdrs[i];
        m_iov[i][0].iov_len  = sizeof(UdpMjpegHdr);
        m_msgs[i].msg_hdr.msg_iov    = m_iov[i].data();
        m_msgs[i].msg_hdr.msg_iovlen = 2;
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

void VideoHandler::tick()
{
    static int tick_no = 0;
    if ((tick_no++ % 400) == 0) 
    {
        std::cout << "[VideoHandler] tick streaming=" << m_streaming
                  << " runRequested=" << m_run_process
                  << " now<retry?=" << (std::chrono::steady_clock::now() < m_next_retry_tp)
                  << " last_dev='" << m_last_dev << "'\n";
    }

    if (!m_run_process)
    { 
        stopStream();
        return;
    }

    // If stream works, check camera
    static auto next_dev_check = std::chrono::steady_clock::now();
    auto now = std::chrono::steady_clock::now();
    if (m_streaming) 
    {
        if (now < next_dev_check) return; // don't check more frequently
        next_dev_check = now + std::chrono::seconds(2);
        std::string dev = findFirstCamera();
        if (!dev.empty() && dev != m_last_dev)
        {
            if (m_verbose) std::cout << "[VideoHandler] Camera changed: " << m_last_dev << " -> " << dev << "\n";
            stopStream();
            m_last_dev = dev;
            m_next_retry_tp = now + std::chrono::milliseconds(200);
        }
        return;
    }

    // If stream doesn't work then return
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

    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(m_v4l2.fd, &rfds);
    timeval tv{0, 5000}; // 5 ms
    int r = ::select(m_v4l2.fd + 1, &rfds, nullptr, nullptr, &tv);
    if (r <= 0 || !FD_ISSET(m_v4l2.fd, &rfds)) return;

    v4l2_buffer b{};
    b.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    b.memory = V4L2_MEMORY_MMAP;
    if (ioctl(m_v4l2.fd, VIDIOC_DQBUF, &b) < 0) 
    {
        if (errno == EAGAIN) return;
        perror("[VideoHandler] DQBUF");
        stopStream();
        m_next_retry_tp = std::chrono::steady_clock::now() + std::chrono::milliseconds(200);
        return;
    }

    // Frame index and size verification
    if (b.index >= static_cast<unsigned>(m_v4l2.nbufs))
    {
        std::cout << "[VideoHandler] DQBUF: index out of range: " << b.index << " / " << m_v4l2.nbufs << "\n";
        // try requeue, yet stream might need restart
        ioctl(m_v4l2.fd, VIDIOC_QBUF, &b);
        return;
    }
    if (b.bytesused > m_v4l2.bufs[b.index].len)
    {
        std::cout << "[VideoHandler] DQBUF: bytesused > buffer (" << b.bytesused
                  << " > " << m_v4l2.bufs[b.index].len << "), dropping frame\n";
        ioctl(m_v4l2.fd, VIDIOC_QBUF, &b);
        return;
    }

    const uint8_t* frame = static_cast<uint8_t*>(m_v4l2.bufs[b.index].start);
    size_t len = b.bytesused;
    const uint64_t ts = now_us();

    // Space to send whole frame (latest-wins)
    const int fps = m_fps > 0 ? m_fps : 30;
    const uint64_t frame_period_us = 1000000ULL / (uint64_t)fps;
    const uint64_t send_deadline_us = ts + frame_period_us;
    bool frame_sent_ok = true;

    // Check if header + PAYLOAD_BYTES <= MAX_PKT
    if (PAYLOAD_BYTES == 0)
    {
        std::cout << "[VideoHandler] Invalid MAX_PKT / header size\n";
        ioctl(m_v4l2.fd, VIDIOC_QBUF, &b);
        stopStream();
        m_next_retry_tp = std::chrono::steady_clock::now() + std::chrono::milliseconds(200);
        return;
    }

    uint16_t frag_cnt = (uint16_t)((len + PAYLOAD_BYTES - 1) / PAYLOAD_BYTES);
    if (frag_cnt == 0) frag_cnt = 1;
    // Sanity check
    if (frag_cnt > m_msgs.size())
    {
        frag_cnt = static_cast<uint16_t>(m_msgs.size());
    }

    // Send whole frame
    {
        m_fd = m_udp->native_fd();

        for (uint16_t i = 0; i < frag_cnt; ++i)
        {
            const size_t off   = size_t(i) * PAYLOAD_BYTES;
            if (off >= len) break;
            const size_t chunk = std::min(PAYLOAD_BYTES, len - off);
            UdpMjpegHdr h{};
            h.magic    = htonl(0x4D4A5047);
            h.seq      = htonl(m_seq);
            h.frag_idx = htons(i);
            h.frag_cnt = htons(frag_cnt);
            h.ts_us    = htobe64(ts);
            m_hdrs[i]  = h;

            m_iov[i][0].iov_base = &m_hdrs[i];
            m_iov[i][0].iov_len  = sizeof(UdpMjpegHdr);
            m_iov[i][1].iov_base = const_cast<uint8_t*>(frame + off);
            m_iov[i][1].iov_len  = chunk;
        }

        int sent_total = 0;
        // simple pacing: after successful sendmmsg short CPU break
        auto tiny_pause = [](){
#ifdef _GNU_SOURCE
            sched_yield();
#else
            struct timespec ts{0, 50000};
            nanosleep(&ts, nullptr); /* 50 µs */
#endif
        };

        uint32_t tx_batches = 0;
        uint32_t tx_eagain = 0;
        const uint64_t send_t0 = now_us();
        bool used_fallback = false;
        bool missed_deadline = false;
        bool had_send_err = false;

        while (sent_total < frag_cnt)
        {
            {
                const uint64_t now = now_us();
                if (now >= send_deadline_us) {
                    frame_sent_ok = false;
                    missed_deadline = true;
                    break;
                }
                const uint64_t us_left = send_deadline_us - now;
                const int frags_left = frag_cnt - sent_total;
                if (us_left < 2000 && frags_left > 4)
                {
                    frame_sent_ok   = false;
                    missed_deadline = true;
                    break;
                }
            }

            int batch = frag_cnt - sent_total;
            if (batch > 16) batch = 16; // don't push all data at once
            int n = ::sendmmsg(m_fd, m_msgs.data() + sent_total, batch, MSG_DONTWAIT);
            if (n > 0) {
                sent_total += n;
                ++tx_batches;
                // short pause for kernel operation
                tiny_pause();
                continue;
            }
            if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
            {
                // light backoff
                ++tx_eagain;
                struct timespec ts{0, 80000}; /* 80 µs */
                nanosleep(&ts, nullptr);
                // check if frame is not too big to finish
                if (now_us() >= send_deadline_us)
                {
                    frame_sent_ok = false;
                    missed_deadline = true;
                    break;
                }
                // start again
                continue;
            }
            if (n < 0 && errno == ENOSYS)
            {
                used_fallback = true;
                for (uint16_t i = sent_total; i < frag_cnt; ++i)
                {
                    msghdr mh{};
                    mh.msg_iov    = m_iov[i].data();
                    mh.msg_iovlen = 2;
                    if (now_us() >= send_deadline_us)
                    {
                        frame_sent_ok = false;
                        missed_deadline = true;
                        break;
                    }
                    ssize_t s = ::sendmsg(m_fd, &mh, MSG_DONTWAIT);
                    if (s < 0) 
                    {
                        if (errno == EAGAIN || errno == EWOULDBLOCK)
                        {
                            ++tx_eagain;
                            struct timespec ts{0, 120000};
                            nanosleep(&ts, nullptr);
                            s = ::sendmsg(m_fd, &mh, MSG_DONTWAIT);
                            if (s < 0)
                            { 
                                had_send_err = true;
                                frame_sent_ok = false;
                                break;
                            }
                        } 
                        else 
                        {
                            had_send_err = true;
                            frame_sent_ok = false;
                            break;
                        }
                    }
                    tiny_pause();
                    ++tx_batches;
                }
                break;
            }
            std::cerr << "[UDP] sendmmsg failed: " << strerror(errno) << "\n";
            frame_sent_ok = false;
            if (now_us() >= send_deadline_us) 
            {
                frame_sent_ok = false;
                missed_deadline = true;
                break;
            }
            break;
        }
        const uint64_t send_us = now_us() - send_t0;
        // Signle log on drop
        if (!frame_sent_ok || sent_total < frag_cnt)
        {
            if (m_verbose)
            {
                std::cerr << "[TX] DROP seq=" << m_seq
                        << " sent=" << sent_total << "/" << frag_cnt
                        << " deadline=" << (missed_deadline?"miss":"ok")
                        << " send_err=" << (had_send_err?"yes":"no")
                        << " eagain=" << tx_eagain
                        << " batches=" << tx_batches
                        << " send_us=" << send_us
                        << "\n";
            }
        }
#if defined(DEBUG)
        g_tx.note((uint32_t)len, (uint32_t)frag_cnt,
                frame_sent_ok && (sent_total == frag_cnt),
                send_us, tx_batches, tx_eagain,
                missed_deadline, had_send_err, used_fallback);
#endif
        ++m_seq;
    }

    if (ioctl(m_v4l2.fd, VIDIOC_QBUF, &b) < 0) 
    {
        perror("[VideoHandler] QBUF");
        stopStream();
        m_next_retry_tp = std::chrono::steady_clock::now() + std::chrono::milliseconds(200);
        return;
    }
}

void VideoHandler::signalCallbackHandler(int)
{
    m_run_process.store(false, std::memory_order_relaxed);
}

} // video_handler
} // odin