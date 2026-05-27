#ifndef UDP_CHANNEL_HPP
#define UDP_CHANNEL_HPP

#include <cstdint>

namespace odin
{
namespace odin_comm
{

enum class UdpChannel : std::uint8_t
{
    Diagnostic,
    ControlSelection,
    ScriptedMotionCommand,
    ScriptedMotionStatus,
    ServoStep,
    Video,
    CameraPosition,
    CameraPositionReady,
    JoypadData,
    LedState
};

} // namespace odin_comm
} // namespace odin

#endif // UDP_CHANNEL_HPP