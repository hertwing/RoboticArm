#ifndef JOYPAD_DATA_HPP
#define JOYPAD_DATA_HPP

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

struct JoypadData
{
    static constexpr std::size_t Size = 7;
    std::array<std::uint8_t, Size> data{};
};

template <>
struct MessageTraits<JoypadData>
{
    static constexpr MessageType messageType = MessageType::JoypadState;
    static constexpr MessageId messageId = MessageId::JoypadData;
};

} // namespace odin_comm
} // namespace odin

#endif // JOYPAD_DATA_HPP