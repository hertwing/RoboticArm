#include "UdpMjpegReceiver.h"

#ifndef Q_MOC_RUN
#include "InetCommData.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <vector>

#include <arpa/inet.h>
#include <endian.h>
#include <errno.h>
#include <netinet/in.h>
#include <sys/uio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#endif

#include <QElapsedTimer>

using namespace odin::video_handler;

static inline bool seq_less(uint32_t a, uint32_t b)
{
    return (int32_t)(a - b) < 0;
}

static inline bool sane_frag_cnt(uint16_t n)
{
    return n > 0 && n <= 2048;
}

void JpegWorker::decode(QByteArray data)
{
#if defined(DEBUG)
    auto t0 = std::chrono::steady_clock::now();
#endif
    QImage img;
    (void)img.loadFromData(reinterpret_cast<const uchar*>(data.constData()),
                           data.size(), "JPG");
    // Input format is RGB32 (640 x 480) - decode it to RGB888
    if (img.format() != QImage::Format_RGB888)
        img = img.convertToFormat(QImage::Format_RGB888);
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
    m_udpRx = std::make_unique<UdpHandler<std::uint8_t>>(MAX_PKT, VIDEO_PORT);
    m_udpRx->set_recv_buffer_bytes(UDP_BUFF);
    m_udpRx->set_dscp(VIDEO_DSCP);
    m_acc.reserve(256);

#if defined(__linux__)
{
    int fd = m_udpRx->native_fd();
    int rcvbuf = UDP_BUFF;
    ::setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &rcvbuf, sizeof(rcvbuf));
}
#endif

    m_jpegThread = new QThread(this);
    m_jpegWorker = new JpegWorker();
    m_jpegWorker->moveToThread(m_jpegThread);
    connect(m_jpegThread, &QThread::finished, m_jpegWorker, &QObject::deleteLater);
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

void UdpMjpegReceiver::start()
{
    Q_ASSERT(thread() == QThread::currentThread());
    if (!m_notifier) 
    {
        // pre-init for recvmmsg
#if defined(__linux__)
        if (m_batchBuf.size() < (size_t)BATCH * MAX_PKT)
            m_batchBuf.resize((size_t)BATCH * MAX_PKT);
        if ((int)m_msgs.size() < BATCH)
        {
            m_msgs.resize(BATCH);
            m_iovec.resize(BATCH);
            m_srcs.resize(BATCH);
            for (int i=0; i<BATCH; ++i)
            {
                m_iovec[i].iov_base = nullptr;
                m_iovec[i].iov_len  = MAX_PKT;
                m_msgs[i].msg_hdr.msg_iov    = &m_iovec[i];
                m_msgs[i].msg_hdr.msg_iovlen = 1;
                m_msgs[i].msg_hdr.msg_name   = &m_srcs[i];
                m_msgs[i].msg_hdr.msg_namelen= sizeof(sockaddr_in);
            }
        }
#endif
        m_notifier = new QSocketNotifier(m_udpRx->native_fd(), QSocketNotifier::Read, this);
        connect(m_notifier, &QSocketNotifier::activated, this, [this]{ pollUdp(); });
        m_notifier->setEnabled(true);
    }
}

void UdpMjpegReceiver::stop()
{
    if (m_notifier) 
    {
        m_notifier->setEnabled(false);
        m_notifier->deleteLater();
        m_notifier = nullptr;
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

    const auto call_t0 = std::chrono::steady_clock::now();
    const auto deadline = call_t0 + std::chrono::microseconds(POLL_SLICE_US);
    auto slice_time_exceeded = [&](){
        return std::chrono::steady_clock::now() >= deadline;
    };

    int processed = 0;
#if defined(__linux__)
    // Fast-path: batch read via recvmmsg()
    while (true)
    {
        int want = std::min(BATCH, RX_BUDGET - processed);
        if (want <= 0) break;

        // set iov_base only (the rest was initiated in start())
        for (int i=0; i<want; ++i)
            m_iovec[i].iov_base = m_batchBuf.data() + (size_t)i * MAX_PKT;

        int nread = ::recvmmsg(m_udpRx->native_fd(), m_msgs.data(), want,
                               MSG_DONTWAIT, nullptr);
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
            const msghdr& mh = m_msgs[i].msg_hdr;
            const ssize_t got = m_msgs[i].msg_len;
            if (got <= 0) continue;
            if ((size_t)got < sizeof(UdpMjpegHdr))
            {
                ++m_rxStats.incompleteFrames;
                ++processed;
                continue;
            }
            if ((size_t)got > MAX_PKT)
            {
                ++m_rxStats.incompleteFrames;
                ++processed;
                continue;
            }

            const uint8_t* buf = static_cast<const uint8_t*>(m_iovec[i].iov_base);
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
                acc.frag.resize(frag_cnt, FragInfo{0,false});
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

            if (!A.frag[frag_idx].got) 
            {
                size_t off = (size_t)frag_idx * MTU;
                if (off + payload_len <= A.data.size()) 
                {
                    std::memcpy(A.data.data() + off, payload, payload_len);
                    A.frag[frag_idx].len = (uint16_t)payload_len;
                    A.frag[frag_idx].got = true;
                    ++A.received;
                }
            }

            if (A.received == A.frag_cnt)
            {
                size_t total = 0;
                for (uint16_t j=0; j<A.frag_cnt; ++j) total += A.frag[j].len;
                Q_ASSERT(total <= std::numeric_limits<int>::max());
                QByteArray ba;
                ba.resize((int)total);
                int pos = 0;
                for (uint16_t j=0; j<A.frag_cnt; ++j)
                {
                    const int len = (int)A.frag[j].len;
                    if (len > 0)
                    {
                        std::memcpy(ba.data()+pos, A.data.data() + (size_t)j*MTU, len);
                        pos += len;
                    }
                }
#if defined(DEBUG)
                m_rxStats.note((uint32_t)pos, (uint32_t)A.frag_cnt);
#endif

                if (!m_decoderBusy.exchange(true, std::memory_order_relaxed))
                {
                    QMetaObject::invokeMethod(m_jpegWorker, "decode",
                                              Qt::QueuedConnection,
                                              Q_ARG(QByteArray, std::move(ba)));
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
    while (true)
    {
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
            acc.frag.resize(frag_cnt, FragInfo{0,false});
            acc.received = 0;
            acc.start = std::chrono::steady_clock::now();
            it = m_acc.emplace(seq, std::move(acc)).first;
        }
        auto& A = it->second;
        if (A.frag_cnt != frag_cnt) continue;

        if (!A.frag[frag_idx].got)
        {
            size_t off = (size_t)frag_idx * MTU;
            if (off + payload_len <= A.data.size())
            {
                std::memcpy(A.data.data() + off, payload, payload_len);
                A.frag[frag_idx].len = (uint16_t)payload_len;
                A.frag[frag_idx].got = true;
                ++A.received;
            }
        }

        if (A.received == A.frag_cnt)
        {
            size_t total = 0;
            for (uint16_t i=0; i<A.frag_cnt; ++i) total += A.frag[i].len;
            Q_ASSERT(total <= std::numeric_limits<int>::max());
            QByteArray ba;
            ba.resize(int(total));
            int pos = 0;
            for (uint16_t i=0; i<A.frag_cnt; ++i)
            {
                int len = (int)A.frag[i].len;
                if (len > 0)
                {
                    std::memcpy(ba.data()+pos, A.data.data() + (size_t)i*MTU, len);
                    pos += len;
                }
            }
            if (!m_decoderBusy.exchange(true, std::memory_order_relaxed))
            {
                QMetaObject::invokeMethod(m_jpegWorker, "decode",
                                          Qt::QueuedConnection,
                                          Q_ARG(QByteArray, std::move(ba)));
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

    // hard limit in-flight without sort, delete oldest
    if (m_acc.size() > (size_t)MAX_INFLIGHT)
    {
        // one run: find oldest and delete until limit
        while (m_acc.size() > (size_t)MAX_INFLIGHT)
        {
            auto oldest = m_acc.end();
            auto oldest_tp = now;
            for (auto it = m_acc.begin(); it != m_acc.end(); ++it)
            {
                if (it->second.start < oldest_tp)
                {
                    oldest_tp = it->second.start;
                    oldest = it;
                }
            }
            if (oldest != m_acc.end())
            {
                ++m_rxStats.incompleteFrames;
                m_acc.erase(oldest);
            } else break;
        }
    }
}