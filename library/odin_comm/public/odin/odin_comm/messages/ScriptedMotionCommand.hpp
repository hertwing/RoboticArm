#ifndef SCRIPTED_MOTION_COMMAND_HPP
#define SCRIPTED_MOTION_COMMAND_HPP

#include <cstdint>

#include "odin/odin_comm/config/MessageId.hpp"
#include "odin/odin_comm/core/MessageType.hpp"
#include "odin/odin_comm/messaging/MessageTraits.hpp"

namespace odin
{
namespace odin_comm
{

enum class ScriptedMotionCommandState : std::uint8_t
{
    Unknown = 0,
    StartRequested = 1,
    ExecuteOnArm = 2,
    RequestComplete = 3,
    StopRequested = 4,
    Error = 5,
};

struct ScriptedMotionCommand
{
    ScriptedMotionCommandState state{ScriptedMotionCommandState::Unknown};
};

template <>
struct MessageTraits<ScriptedMotionCommand>
{
    static constexpr MessageType messageType = MessageType::ScriptedMotionCommand;
    static constexpr MessageId messageId = MessageId::ScriptedMotionCommand;
};

} // namespace odin_comm
} // namespace odin

#endif // SCRIPTED_MOTION_COMMAND_HPP