#ifndef CAMERA_POSITION_HPP
#define CAMERA_POSITION_HPP

#include "odin/odin_comm/config/MessageId.hpp"
#include "odin/odin_comm/core/MessageType.hpp"
#include "odin/odin_comm/messages/ServoStep.hpp"
#include "odin/odin_comm/messaging/MessageTraits.hpp"

namespace odin
{
namespace odin_comm
{

struct CameraPosition
{
    ServoStep pan_position{};
    ServoStep tilt_position{};
    bool target_smiling{};
};

template <>
struct MessageTraits<CameraPosition>
{
    static constexpr MessageType messageType = MessageType::CameraMotionState;
    static constexpr MessageId messageId = MessageId::CameraPosition;
};

} // namespace odin_comm
} // namespace odin

#endif // CAMERA_POSITION_HPP