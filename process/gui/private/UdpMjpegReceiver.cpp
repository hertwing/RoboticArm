#include "UdpMjpegReceiver.h"

#ifndef Q_MOC_RUN
#include "InetCommData.h"

#include <arpa/inet.h>
#include <endian.h>
#include <sys/uio.h>
#include <sys/socket.h>
#include <errno.h>
#include <time.h>

#include <chrono>
#include <cstring>
#include <iostream>
#include <array>
#include <algorithm>
#include <vector>
#endif

#include <QElapsedTimer>

using namespace odin::video_handler;

static constexpr int POLL_SLICE_US = 5000;

static inline bool seq_less(uint32_t a, uint32_t b) {
    return (int32_t)(a - b) < 0;
}

static inline bool sane_frag_cnt(uint16_t n)
{
    return n > 0 && n <= 2048;
}

// ---- JpegWorker slot implementation (now top-level) ----
void JpegWorker::decode(QByteArray data)
{
    #if defined(DEBUG)
    auto t0 = std::chrono::steady_clock::now();
    #endif
    QImage img;
    (void)img.loadFromData(reinterpret_cast<const uchar*>(data.constData()),
                           data.size(), "JPG");
    #if defined(DEBUG)
    auto t1 = std::chrono::steady_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
    static uint64_t cnt=0, sum=0; static int64_t maxms=0; ++cnt; sum+=ms; if(ms>maxms) maxms=ms;
    if (cnt % 30 == 0)
    {
        std::cout << "[DEC] frames=" << cnt << " avg_ms=" << (sum*1.0/cnt)
                  << " max_ms=" << maxms << " last=" << ms << std::endl;
    }
    #endif
    emit decoded(img);
}

UdpMjpegReceiver::UdpMjpegReceiver(QObject* parent) :
    QObject(parent), m_rxBuf(MAX_PKT)
{
    m_udpRx = std::make_unique<UdpHandler<uint8_t>>(MAX_PKT, VIDEO_PORT);
    m_udpRx->set_recv_buffer_bytes(UDP_BUFF);
    m_udpRx->set_dscp(VIDEO_DSCP);

#if defined(__linux__)
{
    int fd = m_udpRx->native_fd();
    int rcvbuf = UDP_BUFF;
    ::setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &rcvbuf, sizeof(rcvbuf));
}
#endif

    m_notifier = new QSocketNotifier(m_udpRx->native_fd(), QSocketNotifier::Read, this);
    connect(m_notifier, &QSocketNotifier::activated, this, [this]{ pollUdp(); });

    // less realocation with big frames
    m_jpgScratch.reserve(JPEG_SCRATCH_RESERVE);

    // start decode thread
    m_jpegThread = new QThread(this);
    m_jpegWorker = new JpegWorker();
    m_jpegWorker->moveToThread(m_jpegThread);
    connect(m_jpegThread, &QThread::finished, m_jpegWorker, &QObject::deleteLater);
    // after docing emit frame to GUI
    connect(m_jpegWorker, &JpegWorker::decoded, this, [this](const QImage& img){
        m_decoderBusy.store(false, std::memory_order_relaxed);
        emit frameReady(img);
    });
    m_jpegThread->start();
}

UdpMjpegReceiver::~UdpMjpegReceiver()
{
    if (m_notifier) m_notifier->setEnabled(false);
    if (m_jpegThread) 
    {
        m_jpegThread->quit();
        m_jpegThread->wait();
    }
}

// For debugging
struct PollStats
{
    uint64_t calls = 0, sum_us = 0, max_us = 0, last_us = 0;
    uint64_t over2ms = 0, over5ms = 0, over10ms = 0;
    void note(uint64_t us) 
    {
        ++calls; sum_us += us; last_us = us; if (us > max_us) max_us = us;
        if (us > 2000) ++over2ms;
        if (us > 5000) ++over5ms;
        if (us > 10000) ++over10ms;
        if (calls % 2000 == 0) 
        {
            std::cout << "[RX] pollUdp_us avg=" << (sum_us / calls)
                      << " max=" << max_us
                      << " last=" << last_us
                      << " | over2ms=" << over2ms
                      << " over5ms=" << over5ms
                      << " over10ms=" << over10ms
                      << std::endl;
        }
    }
};
static PollStats g_pollStats;

void UdpMjpegReceiver::pollUdp()
{
    if (!m_udpRx) return;
    m_notifier->setEnabled(false);

    auto near_done = [&]()
    {
        for (const auto& kv : m_acc) 
        {
            const auto& A = kv.second;
            if (A.frag_cnt >= 10 && (A.frag_cnt - A.received) <= 2) return true;
        }
        return false;
    };

    auto grace_1ms_if_needed = [&]()
    {
        static thread_local auto last_grace = std::chrono::steady_clock::now();
        auto now = std::chrono::steady_clock::now();
        if (near_done() && (now - last_grace) > std::chrono::milliseconds(5))
        {
            // 1 ms grace
            struct timespec ts{0, 1'000'000};
            nanosleep(&ts, nullptr);
            last_grace = std::chrono::steady_clock::now();
            return true;
        }
        return false;
    };

    const auto call_t0 = std::chrono::steady_clock::now();
    const auto deadline = call_t0 + std::chrono::microseconds(POLL_SLICE_US);
    auto slice_time_exceeded = [&](){
        return std::chrono::steady_clock::now() >= deadline;
    };

    int processed = 0;
#if defined(__linux__)
    // ---- Fast-path: batch read via recvmmsg() ----

    // Buffer (BATCH * MAX_PKT)
    static thread_local std::vector<uint8_t> batchBuf;
    if (batchBuf.size() < (size_t)BATCH * MAX_PKT)
        batchBuf.resize((size_t)BATCH * MAX_PKT);

    static thread_local std::vector<mmsghdr>     msgs;
    static thread_local std::vector<iovec>       iov;
    static thread_local std::vector<sockaddr_in> srcs;

    if ((int)msgs.size() < BATCH)
    {
        msgs.resize(BATCH);
        iov.resize(BATCH);
        srcs.resize(BATCH);
    }

    for (;;) 
    {
        if (slice_time_exceeded())
        {
            if (!grace_1ms_if_needed()) break;
        }
        int want = std::min(BATCH, RX_BUDGET - processed);
        if (want <= 0) break;

        // prepare structures
        for (int i=0; i<want; ++i)
        {
            std::memset(&msgs[i], 0, sizeof(mmsghdr));
            std::memset(&srcs[i], 0, sizeof(sockaddr_in));
            iov[i].iov_base = batchBuf.data() + (size_t)i * MAX_PKT;
            iov[i].iov_len  = MAX_PKT;
            msgs[i].msg_hdr.msg_iov     = &iov[i];
            msgs[i].msg_hdr.msg_iovlen  = 1;
            msgs[i].msg_hdr.msg_name    = &srcs[i];
            msgs[i].msg_hdr.msg_namelen = sizeof(sockaddr_in);
        }

        int nread = ::recvmmsg(m_udpRx->native_fd(), msgs.data(), want,
                               MSG_DONTWAIT | MSG_TRUNC, nullptr);
        if (nread < 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK) break;
            if (errno == EINTR) continue;
            std::cout << "[RX] recvmmsg error: " << strerror(errno) << "\n";
            break;
        }
        if (nread == 0) break;

        for (int i=0; i<nread; ++i) 
        {
            if (slice_time_exceeded())
            {
                if (!grace_1ms_if_needed()) goto end_linux_loop;
            }
            const msghdr& mh = msgs[i].msg_hdr;
            const ssize_t got = msgs[i].msg_len;
            if (got <= 0) continue;
            if ((size_t)got < sizeof(UdpMjpegHdr))
            {
                ++m_rxStats.incompleteFrames;
                ++processed;
                continue;
            }
            if ((size_t)got > MAX_PKT || (mh.msg_flags & MSG_TRUNC))
            {
                ++m_rxStats.incompleteFrames;
                ++processed;
                continue;
            }

            const uint8_t* buf = static_cast<const uint8_t*>(iov[i].iov_base);
            const auto* hdr = reinterpret_cast<const UdpMjpegHdr*>(buf);
            if (ntohl(hdr->magic) != 0x4D4A5047) 
            {
                ++m_rxStats.incompleteFrames;
                ++processed;
                continue; 
            } // 'MJPG'

            const uint32_t seq      = ntohl(hdr->seq);
            const uint16_t frag_idx = ntohs(hdr->frag_idx);
            const uint16_t frag_cnt = ntohs(hdr->frag_cnt);
            const uint64_t ts_us    = be64toh(hdr->ts_us);
            const uint16_t w        = ntohs(hdr->width);
            const uint16_t h        = ntohs(hdr->height);

            if (!sane_frag_cnt(frag_cnt) || frag_idx >= frag_cnt) 
            {
                ++m_rxStats.incompleteFrames;
                ++processed;
                continue;
            }

            const size_t payload_len = (size_t)got - sizeof(UdpMjpegHdr);
            const uint8_t* payload   = buf + sizeof(UdpMjpegHdr);
            if (payload_len > MTU) 
            { 
                ++m_rxStats.incompleteFrames; 
                ++processed; 
                continue; 
            }

            auto it = m_acc.find(seq);
            if (it == m_acc.end()) 
            {
                FragAcc acc;
                acc.frag_cnt = frag_cnt;
                acc.data.resize((size_t)frag_cnt * MTU);
                acc.frag_len.assign(frag_cnt, 0);
                acc.got.assign(frag_cnt, false);
                acc.ts_us = ts_us; acc.w = w; acc.h = h;
                acc.received = 0;
                acc.start = std::chrono::steady_clock::now();
                it = m_acc.emplace(seq, std::move(acc)).first;
            }
            auto& A = it->second;
            if (A.frag_cnt != frag_cnt) 
            { 
                ++m_rxStats.incompleteFrames; 
                ++processed; 
                continue; 
            }

            if (!A.got[frag_idx]) 
            {
                size_t off = (size_t)frag_idx * MTU;
                if (off + payload_len <= A.data.size()) 
                {
                    std::memcpy(A.data.data() + off, payload, payload_len);
                    A.frag_len[frag_idx] = (uint16_t)payload_len;
                    A.got[frag_idx] = true;
                    ++A.received;
                }
            }

            if (A.received == A.frag_cnt)
            {
                size_t total = 0;
                for (uint16_t j=0; j<A.frag_cnt; ++j) total += A.frag_len[j];
                if (m_jpgScratch.size() < total) m_jpgScratch.resize(total);
                int pos = 0;
                for (uint16_t j=0; j<A.frag_cnt; ++j)
                {
                    size_t off = (size_t)j * MTU;
                    int l = (int)A.frag_len[j];
                    if (l > 0)
                    { 
                        std::memcpy(m_jpgScratch.data()+pos, A.data.data()+off, l); 
                        pos += l; 
                    }
                }
                #if defined(DEBUG)
                m_rxStats.note((uint32_t)pos, (uint32_t)A.frag_cnt);
                #endif

                if (!m_decoderBusy.exchange(true, std::memory_order_relaxed))
                {
                    QByteArray ba(reinterpret_cast<const char*>(m_jpgScratch.data()), pos);
                    QMetaObject::invokeMethod(m_jpegWorker, "decode",
                                              Qt::QueuedConnection,
                                              Q_ARG(QByteArray, ba));
                    m_lastDisplayedSeq = seq;
                }
                m_acc.erase(it);
            }
            ++processed;
            if (processed >= RX_BUDGET) break;
        }
        if (processed >= RX_BUDGET || slice_time_exceeded()) break;
    }
end_linux_loop:
#else
    for (;;)
    {
        if (slice_time_exceeded())
        {
            if (!grace_1ms_if_needed()) break;
        }
        size_t got = 0;
        std::int8_t rc = m_udpRx->read(m_rxBuf.data(), m_rxBuf.size(), got, 0);
        if (rc == 0) break;
        if (rc == -1)
        {
            std::cout << "[RX] UDP poll error.\n"; 
            break;
        }
        if (got < sizeof(UdpMjpegHdr))
        { 
            ++processed; 
            continue; 
        }

        auto* hdr = reinterpret_cast<const UdpMjpegHdr*>(m_rxBuf.data());
        if (ntohl(hdr->magic) != 0x4D4A5047) continue; // 'MJPG'

        const uint32_t seq      = ntohl(hdr->seq);
        const uint16_t frag_idx = ntohs(hdr->frag_idx);
        const uint16_t frag_cnt = ntohs(hdr->frag_cnt);
        const uint64_t ts_us    = be64toh(hdr->ts_us);
        const uint16_t w        = ntohs(hdr->width);
        const uint16_t h        = ntohs(hdr->height);

        if (!sane_frag_cnt(frag_cnt) || frag_idx >= frag_cnt)
        { 
            ++processed;
            continue;
        }

        const size_t payload_len = got - sizeof(UdpMjpegHdr);
        const uint8_t* payload   = m_rxBuf.data() + sizeof(UdpMjpegHdr);
        if (payload_len > MTU)
        { 
            ++processed; 
            continue; 
        }

        auto it = m_acc.find(seq);
        if (it == m_acc.end())
        {
            FragAcc acc;
            acc.frag_cnt = frag_cnt;
            acc.data.resize((size_t)frag_cnt * MTU);
            acc.frag_len.assign(frag_cnt, 0);
            acc.got.assign(frag_cnt, false);
            acc.ts_us = ts_us; acc.w = w; acc.h = h;
            acc.received = 0;
            acc.start = std::chrono::steady_clock::now();
            it = m_acc.emplace(seq, std::move(acc)).first;
        }
        auto& A = it->second;
        if (A.frag_cnt != frag_cnt) continue;

        if (!A.got[frag_idx])
        {
            size_t off = (size_t)frag_idx * MTU;
            if (off + payload_len <= A.data.size())
            {
                std::memcpy(A.data.data() + off, payload, payload_len);
                A.frag_len[frag_idx] = (uint16_t)payload_len;
                A.got[frag_idx] = true;
                ++A.received;
            }
        }

        if (A.received == A.frag_cnt)
        {
            size_t total = 0;
            for (uint16_t i=0; i<A.frag_cnt; ++i) total += A.frag_len[i];

            if (m_jpgScratch.size() < total) m_jpgScratch.resize(total);
            int pos = 0;
            for (uint16_t i=0; i<A.frag_cnt; ++i)
            {
                size_t off = (size_t)i * MTU;
                int len = (int)A.frag_len[i];
                if (len > 0)
                { 
                    std::memcpy(m_jpgScratch.data()+pos, A.data.data()+off, len); 
                    pos += len;
                }
            }

            // Offload JPEG decode for worker-thread; if it's busy drop frame
            if (!m_decoderBusy.exchange(true, std::memory_order_relaxed))
            {
                QByteArray ba(reinterpret_cast<const char*>(m_jpgScratch.data()), pos);
                QMetaObject::invokeMethod(m_jpegWorker, "decode",
                                          Qt::QueuedConnection,
                                          Q_ARG(QByteArray, ba));
                m_lastDisplayedSeq = seq;
            }
            m_acc.erase(it);
        }
        if (++processed >= RX_BUDGET) break;
    }
#endif

    dropStaleFrames();

    const auto call_t1 = std::chrono::steady_clock::now();
    const uint64_t us_total =
        (uint64_t)std::chrono::duration_cast<std::chrono::microseconds>(call_t1 - call_t0).count();
    #if defined(DEBUG)
    g_pollStats.note(us_total);
    #endif

    m_notifier->setEnabled(true);
}

void UdpMjpegReceiver::dropStaleFrames()
{
    const auto now = std::chrono::steady_clock::now();
    const auto staleAfter = std::chrono::milliseconds(1000);

    std::vector<uint32_t> toErase;
    toErase.reserve(m_acc.size());
    for (const auto & [seq, A] : m_acc)
    {
        if (now - A.start > staleAfter)
        { 
            toErase.push_back(seq); 
            continue; 
        }
        if (m_lastDisplayedSeq && seq_less(seq + BATCH, m_lastDisplayedSeq)) toErase.push_back(seq);
    }

    for (const auto & s : toErase)
    {
        auto it = m_acc.find(s);
        if (it != m_acc.end())
        {
            ++m_rxStats.incompleteFrames; // STAT
            m_acc.erase(it);
        }
    }

    if (m_acc.size() > BATCH)
    {
        std::vector<std::pair<std::chrono::steady_clock::time_point,uint32_t>> ages;
        ages.reserve(m_acc.size());
        for (auto& [seq, A] : m_acc) ages.emplace_back(A.start, seq);
        std::sort(ages.begin(), ages.end());
        for (size_t i=0; i + BATCH < ages.size(); ++i)
        {
            ++m_rxStats.incompleteFrames;
            m_acc.erase(ages[i].second);
        }
    }
}