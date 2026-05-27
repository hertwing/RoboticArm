#ifndef TOPIC_ID_HPP
#define TOPIC_ID_HPP

#include <cstdint>

namespace odin
{
namespace odin_comm
{

enum class TopicId : std::uint16_t
{
    Unknown = 0,

    ControlSelection = 1,
    DiagnosticState = 2,
    JoypadData = 3,
    LedState = 4,

    ScriptedMotionCommand = 10,
    ScriptedMotionStatus = 11,
    ServoStep = 12,

    CameraPosition = 20,
    CameraPositionReady = 21,

    VideoFrame = 30
};

} // namespace odin_comm
} // namespace odin

#endif // TOPIC_ID_HPP