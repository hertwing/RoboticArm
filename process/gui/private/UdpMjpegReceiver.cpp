#include "UdpMjpegReceiver.h"

#include "InetCommData.h"

#include <arpa/inet.h>
#include <endian.h>
#include <cstring>
#include <iostream>

using namespace odin::video_handler;

UdpMjpegReceiver::UdpMjpegReceiver(QObject* parent) :
    QObject(parent), m_rxBuf(MAX_PKT)
{
    m_udpRx = std::make_unique<UdpHandler<uint8_t>>(MAX_PKT, VIDEO_PORT);
    m_udpRx->set_recv_buffer_bytes(4 * 1024 * 1024);
    m_udpRx->set_dscp(8);

    m_notifier = new QSocketNotifier(m_udpRx->native_fd(), QSocketNotifier::Read, this);
    connect(m_notifier, &QSocketNotifier::activated, this, [this]{ pollUdp(); });
}

UdpMjpegReceiver::~UdpMjpegReceiver()
{
    if (m_notifier) m_notifier->setEnabled(false);
}

void UdpMjpegReceiver::pollUdp()
{
    if (!m_udpRx) return;
    m_notifier->setEnabled(false);

    int processed = 0;
    for (;;)
    {
        size_t got = 0;
        std::int8_t rc = m_udpRx->read(m_rxBuf.data(), m_rxBuf.size(), got, 0);
        if (rc == 0) break;
        if (rc == -1) { std::cout << "[RX] UDP poll error.\n"; break; }
        if (got < sizeof(UdpMjpegHdr)) continue;

        auto* hdr = reinterpret_cast<const UdpMjpegHdr*>(m_rxBuf.data());
        if (ntohl(hdr->magic) != 0x4D4A5047) continue; // 'MJPG'

        const uint32_t seq      = ntohl(hdr->seq);
        const uint16_t frag_idx = ntohs(hdr->frag_idx);
        const uint16_t frag_cnt = ntohs(hdr->frag_cnt);
        const uint64_t ts_us    = be64toh(hdr->ts_us);
        const uint16_t w        = ntohs(hdr->width);
        const uint16_t h        = ntohs(hdr->height);

        if (!frag_cnt || frag_idx >= frag_cnt) continue;

        const size_t payload_len = got - sizeof(UdpMjpegHdr);
        const uint8_t* payload   = m_rxBuf.data() + sizeof(UdpMjpegHdr);

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
                if (len > 0) { std::memcpy(m_jpgScratch.data()+pos, A.data.data()+off, len); pos += len; }
            }

            QImage img;
            if (img.loadFromData(m_jpgScratch.data(), pos, "JPG")) {
                emit frameReady(img);
                m_lastDisplayedSeq = seq;
            } else {
                std::cout << "[RX] JPEG decode failed, total=" << pos
                          << " frags=" << A.frag_cnt << " w=" << A.w << " h=" << A.h << "\n";
            }
            m_acc.erase(it);
        }
        if (++processed >= RX_BUDGET) break;
    }

    dropStaleFrames();
    m_notifier->setEnabled(true);
}

void UdpMjpegReceiver::dropStaleFrames()
{
    const auto now = std::chrono::steady_clock::now();
    const auto staleAfter = std::chrono::milliseconds(200);

    std::vector<uint32_t> toErase;
    toErase.reserve(m_acc.size());
    for (const auto & [seq, A] : m_acc)
    {
        if (now - A.start > staleAfter)
        { 
            toErase.push_back(seq); 
            continue; 
        }
        if (m_lastDisplayedSeq && seq + 4 < m_lastDisplayedSeq) toErase.push_back(seq);
    }

    for (const auto & s : toErase) m_acc.erase(s);

    if (m_acc.size() > 8)
    {
        std::vector<std::pair<std::chrono::steady_clock::time_point,uint32_t>> ages;
        ages.reserve(m_acc.size());
        for (auto& [seq, A] : m_acc) ages.emplace_back(A.start, seq);
        std::sort(ages.begin(), ages.end());
        for (size_t i=0; i + 8 < ages.size(); ++i) m_acc.erase(ages[i].second);
    }
}