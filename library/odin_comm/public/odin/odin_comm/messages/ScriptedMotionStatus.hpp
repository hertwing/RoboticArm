#ifndef SCRIPTED_MOTION_STATUS_HPP
#define SCRIPTED_MOTION_STATUS_HPP

#include <cstdint>

#include "odin/odin_comm/config/MessageId.hpp"
#include "odin/odin_comm/core/MessageType.hpp"
#include "odin/odin_comm/messaging/MessageTraits.hpp"

namespace odin
{
namespace odin_comm
{

enum class ScriptedMotionStatusState : std::uint8_t
{
    Unknown = 0,
    Waiting = 1,
    InProgress = 2,
    Completed = 3,
    Error = 4,
    Disconnected = 5,
};

struct ScriptedMotionStatus
{
    ScriptedMotionStatusState state{ScriptedMotionStatusState::Unknown};
};

template <>
struct MessageTraits<ScriptedMotionStatus>
{
    static constexpr MessageType messageType = MessageType::ScriptedMotionStatus;
    static constexpr MessageId messageId = MessageId::ScriptedMotionStatus;
};

} // namespace odin_comm
} // namespace odin

#endif // SCRIPTED_MOTION_STATUS_HPP