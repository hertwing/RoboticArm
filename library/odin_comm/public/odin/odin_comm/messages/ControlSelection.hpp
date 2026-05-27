#ifndef CONTROL_SELECTION_HPP
#define CONTROL_SELECTION_HPP

#include <cstdint>

#include "odin/odin_comm/config/MessageId.hpp"
#include "odin/odin_comm/core/MessageType.hpp"
#include "odin/odin_comm/messaging/MessageTraits.hpp"

namespace odin
{
namespace odin_comm
{

enum class ControlMode : std::uint32_t
{
    Unknown = 0,
    Manual = 1,
    ScriptedMotion = 2,
    CameraControl = 3,
    Diagnostic = 4,
    EmergencyStop = 5
};

struct ControlSelection
{
    ControlMode selectedMode{ControlMode::Unknown};
};

template <>
struct MessageTraits<ControlSelection>
{
    static constexpr MessageType messageType = MessageType::ControlStateSelected;
    static constexpr MessageId messageId = MessageId::ControlSelection;
};

} // namespace odin_comm
} // namespace odin

#endif // CONTROL_SELECTION_HPP