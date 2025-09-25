#ifndef UDPMJPEGRECEIVER_H
#define UDPMJPEGRECEIVER_H

#ifndef Q_MOC_RUN
#include "UdpHandler.hpp"
#include "odin/video_handler/DataTypes.h"

#include <unordered_map>
#include <chrono>
#include <memory>

#include <cstdint>
#include <limits>
#include <chrono>
#include <iostream>
#endif

#include <QObject>
#include <QSocketNotifier>
#include <QImage>
#include <QThread>
#include <QByteArray>


struct RxFrameStats 
{
    uint64_t count = 0;
    uint64_t sumBytes = 0;
    uint32_t minBytes = std::numeric_limits<uint32_t>::max();
    uint32_t maxBytes = 0;
    uint64_t over200k = 0, over500k = 0, over1m = 0;
    uint64_t sumFrags = 0, maxFrags = 0;

    uint64_t incompleteFrames = 0;
    std::chrono::steady_clock::time_point start;

    RxFrameStats()
    { 
        start = std::chrono::steady_clock::now();
    }

    void note(uint32_t bytes, uint32_t frags)
    {
        ++count; sumBytes += bytes; sumFrags += frags;
        minBytes = std::min(minBytes, bytes);
        maxBytes = std::max(maxBytes, bytes);
        maxFrags = std::max<uint64_t>(maxFrags, frags);
        if (bytes > 200*1024) ++over200k;
        if (bytes > 500*1024) ++over500k;
        if (bytes > 1024*1024) ++over1m;

        auto now = std::chrono::steady_clock::now();
        auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count();
        if (elapsed_ms >= 2000)
        {
            double avgB = count ? double(sumBytes)/double(count) : 0.0;
            double avgFr = count ? double(sumFrags)/double(count) : 0.0;
            double fps = elapsed_ms ? (count * 1000.0 / elapsed_ms) : 0.0;
            std::cout << "[RX] window_ms=" << elapsed_ms
                      << " frames=" << count
                      << " fps=" << fps
                      << " avg=" << (uint32_t)avgB << "B"
                      << " min=" << (minBytes==std::numeric_limits<uint32_t>::max()?0:minBytes) << "B"
                      << " max=" << maxBytes << "B"
                      << " avg_frags=" << avgFr
                      << " max_frags=" << maxFrags
                      << " | incomplete=" << incompleteFrames
                      << " over200k=" << over200k
                      << " over500k=" << over500k
                      << " over1m=" << over1m
                      << std::endl;
            *this = RxFrameStats(); // reset + restart
        }
    }
};

// ---- Top-level QObject for JPEG decoding (moc does not support nested Q_OBJECT) ----
class JpegWorker : public QObject
{
    Q_OBJECT
public slots:
    void decode(QByteArray data);
signals:
    void decoded(const QImage& img);
};

class UdpMjpegReceiver : public QObject
{
    Q_OBJECT
public:
    explicit UdpMjpegReceiver(QObject* parent=nullptr);
    ~UdpMjpegReceiver();

signals:
    void frameReady(const QImage&);

private:
    void pollUdp();
    void dropStaleFrames();

    static constexpr std::uint16_t RX_BUDGET = 1024;
    static constexpr std::uint32_t JPEG_SCRATCH_RESERVE= 512 * 1024;
    const int BATCH = 64;
    std::unique_ptr<UdpHandler<uint8_t>> m_udpRx;
    QSocketNotifier* m_notifier = nullptr;
    std::vector<uint8_t> m_rxBuf;
    std::vector<uint8_t> m_jpgScratch;

    RxFrameStats m_rxStats;

    struct FragAcc
    {
        uint16_t frag_cnt{0};
        std::vector<uint8_t> data;
        std::vector<uint16_t> frag_len;
        std::vector<bool>     got;
        uint16_t received{0};
        uint64_t ts_us{0};
        uint16_t w{0}, h{0};
        std::chrono::steady_clock::time_point start;
    };

    std::unordered_map<uint32_t, FragAcc> m_acc;
    uint32_t m_lastDisplayedSeq{0};

    QThread*    m_jpegThread = nullptr;
    JpegWorker* m_jpegWorker = nullptr;
    std::atomic_bool m_decoderBusy{false}; // drop-frames if worker is busy
};

#endif // UDPMJPEGRECEIVER_H