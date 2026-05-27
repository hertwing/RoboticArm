#ifndef CAMERA_POSITION_READY_HPP
#define CAMERA_POSITION_READY_HPP

#include "odin/odin_comm/config/MessageId.hpp"
#include "odin/odin_comm/core/MessageType.hpp"
#include "odin/odin_comm/messages/ServoStep.hpp"
#include "odin/odin_comm/messaging/MessageTraits.hpp"

namespace odin
{
namespace odin_comm
{

struct CameraPositionReady
{
    ServoStep pan_position{};
    ServoStep tilt_position{};
    bool ready{};
};

template <>
struct MessageTraits<CameraPositionReady>
{
    static constexpr MessageType messageType = MessageType::CameraMotionState;
    static constexpr MessageId messageId = MessageId::CameraPositionReady;
};

} // namespace odin_comm
} // namespace odin

#endif // CAMERA_POSITION_READY_HPP