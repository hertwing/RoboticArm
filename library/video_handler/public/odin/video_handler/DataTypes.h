#ifndef VIDEOHANDLER_DATATYPES_H
#define VIDEOHANDLER_DATATYPES_H

#include <cstdint>
#include <string>

namespace odin
{
namespace video_handler
{
    static constexpr std::uint16_t VIDEO_WIDTH = 720;
    static constexpr std::uint16_t VIDEO_HEIGHT = 480;
    static constexpr std::uint8_t VIDEO_FPS = 30;
    const std::string VIDEO_CODEC = "mjpeg";
    const std::string VIDEO_SRC = "auto";

    static constexpr int VIDEO_DSCP = 34;

    #pragma pack(push, 1)
    struct UdpMjpegHdr
    {
        uint32_t magic;      // 'MJPG' = 0x4D4A5047
        uint32_t seq;        // frame num
        uint16_t frag_idx;   // fragment index [0..frag_cnt-1]
        uint16_t frag_cnt;   // how many fragments in frame
        uint64_t ts_us;      // timestamp (monotonic) µs
        uint16_t width;
        uint16_t height;
    };
    #pragma pack(pop)

    static_assert(sizeof(UdpMjpegHdr) == 24, "Unexpected UdpMjpegHdr size");

    struct V4L2Ctx
    {
        int fd = -1;
        struct Buffer { void* start=nullptr; size_t len=0; } bufs[8];
        int nbufs = 0;
        uint16_t width=0, height=0;
    };

    static constexpr size_t MTU = 1400;
    static constexpr size_t MAX_PKT = sizeof(UdpMjpegHdr) + MTU;

    static constexpr size_t PAYLOAD_BYTES = (MAX_PKT > sizeof(UdpMjpegHdr))
                                          ? (MAX_PKT - sizeof(UdpMjpegHdr))
                                          : 0;
} // video_handler
} // odin

#endif