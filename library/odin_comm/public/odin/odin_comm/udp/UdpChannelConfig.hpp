#ifndef UDP_CHANNEL_CONFIG_HPP
#define UDP_CHANNEL_CONFIG_HPP

#include <cstdint>
#include <string>

#include "odin/odin_comm/udp/NetworkDefaults.hpp"
#include "odin/odin_comm/udp/UdpChannel.hpp"
#include "odin/odin_comm/udp/UdpTransportConfig.hpp"

namespace odin
{
namespace odin_comm
{

[[nodiscard]] constexpr std::uint16_t portForChannel(UdpChannel channel) noexcept
{
    switch (channel) {
        case UdpChannel::Diagnostic:
            return DiagnosticSocketPort;

        case UdpChannel::ControlSelection:
            return ControlSelectionPort;

        case UdpChannel::ScriptedMotionCommand:
            return ScriptedMotionCommandPort;

        case UdpChannel::ScriptedMotionStatus:
            return ScriptedMotionStatusPort;

        case UdpChannel::ServoStep:
            return ServoStepPort;

        case UdpChannel::Video:
            return VideoPort;

        case UdpChannel::CameraPosition:
            return CameraPositionPort;

        case UdpChannel::CameraPositionReady:
            return CameraPositionReadyPort;

        case UdpChannel::JoypadData:
            return JoypadDataPort;

        case UdpChannel::LedState:
            return LedStatePort;
    }

    return 0;
}

[[nodiscard]] inline UdpTransportConfig makeGuiToArmConfig(UdpChannel channel)
{
    const auto port = portForChannel(channel);

    return UdpTransportConfig{
        .localPort = port,
        .remoteEndpoint = UdpEndpoint{
            .address = std::string{RoboticArmIp},
            .port = port
        }
    };
}

[[nodiscard]] inline UdpTransportConfig makeArmToGuiConfig(UdpChannel channel)
{
    const auto port = portForChannel(channel);

    return UdpTransportConfig{
        .localPort = port,
        .remoteEndpoint = UdpEndpoint{
            .address = std::string{RoboticGuiIp},
            .port = port
        }
    };
}

} // namespace odin_comm
} // namespace odin

#endif // UDP_CHANNEL_CONFIG_HPP