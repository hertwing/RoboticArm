#ifndef SERVO_STEP_HPP
#define SERVO_STEP_HPP

#include <cstdint>

#include "odin/odin_comm/config/MessageId.hpp"
#include "odin/odin_comm/core/MessageType.hpp"
#include "odin/odin_comm/messaging/MessageTraits.hpp"

namespace odin
{
namespace odin_comm
{

struct ServoStep
{
    std::uint64_t step_number{};
    std::uint8_t servo_number{};
    std::uint16_t position{};
    std::uint64_t delay{};
    std::uint8_t speed{};
};

template <>
struct MessageTraits<ServoStep>
{
    static constexpr MessageType messageType = MessageType::ServoStep;
    static constexpr MessageId messageId = MessageId::ServoStep;
};

} // namespace odin_comm
} // namespace odin

#endif // SERVO_STEP_HPP