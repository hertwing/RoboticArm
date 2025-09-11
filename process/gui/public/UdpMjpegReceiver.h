#ifndef UDPMJPEGRECEIVER_H
#define UDPMJPEGRECEIVER_H

#include "UdpHandler.hpp"
#include "odin/video_handler/DataTypes.h"

#include <QObject>
#include <QSocketNotifier>
#include <QImage>
#include <unordered_map>
#include <chrono>
#include <memory>

class UdpMjpegReceiver : public QObject {
    Q_OBJECT
public:
    explicit UdpMjpegReceiver(QObject* parent=nullptr);
    ~UdpMjpegReceiver();

signals:
    void frameReady(const QImage&);

private:
    void pollUdp();
    void dropStaleFrames();

    static constexpr int RX_BUDGET = 2048;
    std::unique_ptr<UdpHandler<uint8_t>> m_udpRx;
    QSocketNotifier* m_notifier = nullptr;
    std::vector<uint8_t> m_rxBuf;
    std::vector<uint8_t> m_jpgScratch;

    struct FragAcc {
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
};

#endif // UDPMJPEGRECEIVER_H