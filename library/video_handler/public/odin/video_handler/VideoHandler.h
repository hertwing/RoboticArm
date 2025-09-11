#ifndef VIDEOHANDLER_H
#define VIDEOHANDLER_H

#include "odin/video_handler/DataTypes.h"
#include "UdpHandler.hpp"

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>


namespace odin
{
namespace video_handler
{

class VideoHandler
{
public:
    VideoHandler();
    ~VideoHandler() = default;

    void requestRun(bool on);
    bool desired() const;

    void tick();
    void run();

    static void signalCallbackHandler(int signum);
private:
    bool isCaptureDevice(const std::string & dev, std::string * card_out = nullptr);
    std::string findFirstCamera();

    // lifecycle
    bool startStream(const std::string& dev); // init V4L2 + UDP
    void stopStream(); 
private:
    bool m_streaming{false};
    std::string m_last_dev;
    std::chrono::steady_clock::time_point m_next_retry_tp;

    V4L2Ctx m_v4l2{};

    std::unique_ptr<UdpHandler<uint8_t>> m_udp;
    std::vector<uint8_t> m_pkt = std::vector<uint8_t>(MAX_PKT);
    uint32_t m_seq{0};

    const std::uint16_t m_width;
    const std::uint16_t m_height;
    const std::uint8_t  m_fps;
    const std::string   m_codec;
    const std::string   m_src;
    bool m_verbose;
    static bool m_run_process;
};

} // video_handler
} // odin

#endif