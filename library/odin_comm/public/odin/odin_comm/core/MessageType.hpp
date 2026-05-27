#ifndef MESSAGE_TYPE_HPP
#define MESSAGE_TYPE_HPP

#include <cstdint>

namespace odin
{
namespace odin_comm
{

enum class MessageType : std::uint16_t
{
    Unknown = 0,

    Ack = 1,
    Error = 2,
    Heartbeat = 3,

    ControlStateSelected = 100,
    DiagnosticData = 101,
    CameraMotionState = 102,
    JoypadState = 103,
    LedState = 104,

    ScriptedMotionCommand = 200,
    ScriptedMotionStatus = 201,
    ServoStep = 202,

    VideoFrameChunk = 300,
};

} // namespace odin_comm
} // namespace odin

#endif // MESSAGE_TYPE_HPP