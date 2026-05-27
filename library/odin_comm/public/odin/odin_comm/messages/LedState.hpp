#ifndef LED_STATE_HPP
#define LED_STATE_HPP

#include <array>
#include <cstddef>
#include <cstdint>

#include "odin/odin_comm/config/MessageId.hpp"
#include "odin/odin_comm/core/MessageType.hpp"
#include "odin/odin_comm/messaging/MessageTraits.hpp"

namespace odin
{
namespace odin_comm
{

struct LedState
{
    static constexpr std::size_t LedCount = 6;

    std::array<std::uint32_t, LedCount> colors{};
};

template <>
struct MessageTraits<LedState>
{
    static constexpr MessageType messageType = MessageType::LedState;
    static constexpr MessageId messageId = MessageId::LedState;
};

} // namespace odin_comm
} // namespace odin

#endif // LED_STATE_HPP