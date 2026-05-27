#ifndef UDP_TRANSPORT_CONFIG_HPP
#define UDP_TRANSPORT_CONFIG_HPP

#include <cstddef>
#include <cstdint>

#include "odin/odin_comm/core/Types.hpp"
#include "odin/odin_comm/udp/UdpEndpoint.hpp"

namespace odin
{
namespace odin_comm
{

struct UdpTransportConfig
{
    std::uint16_t localPort{0};
    UdpEndpoint remoteEndpoint{};

    std::size_t maxDatagramSize{
        static_cast<std::size_t>(EncodedMessageHeaderSize) + DefaultMaxPayloadSize
    };

    bool bindToAnyAddress{true};
};

} // namespace odin_comm
} // namespace odin

#endif // UDP_TRANSPORT_CONFIG_HPP