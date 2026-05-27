#ifndef MESSAGE_FLAGS_HPP
#define MESSAGE_FLAGS_HPP

#include <cstdint>

namespace odin
{
namespace odin_comm
{

enum class MessageFlags : std::uint16_t {
    None        = 0,
    AckRequired = 1 << 0,
    IsAck       = 1 << 1,
    IsResponse  = 1 << 2,
    IsError     = 1 << 3,
};

[[nodiscard]] constexpr MessageFlags operator|(
    MessageFlags lhs,
    MessageFlags rhs
) noexcept
{
    return static_cast<MessageFlags>(
        static_cast<std::uint16_t>(lhs) |
        static_cast<std::uint16_t>(rhs)
    );
}

[[nodiscard]] constexpr MessageFlags operator&(
    MessageFlags lhs,
    MessageFlags rhs
) noexcept
{
    return static_cast<MessageFlags>(
        static_cast<std::uint16_t>(lhs) &
        static_cast<std::uint16_t>(rhs)
    );
}

constexpr MessageFlags& operator|=(
    MessageFlags& lhs,
    MessageFlags rhs
) noexcept
{
    lhs = lhs | rhs;
    return lhs;
}

[[nodiscard]] constexpr bool hasFlag(
    MessageFlags flags,
    MessageFlags flag
) noexcept
{
    return (flags & flag) != MessageFlags::None;
}

[[nodiscard]] constexpr bool requiresAck(MessageFlags flags) noexcept
{
    return hasFlag(flags, MessageFlags::AckRequired);
}

[[nodiscard]] constexpr bool isAck(MessageFlags flags) noexcept
{
    return hasFlag(flags, MessageFlags::IsAck);
}

[[nodiscard]] constexpr bool isResponse(MessageFlags flags) noexcept
{
    return hasFlag(flags, MessageFlags::IsResponse);
}

[[nodiscard]] constexpr bool isError(MessageFlags flags) noexcept
{
    return hasFlag(flags, MessageFlags::IsError);
}

} // namespace odin_comm
} // namespace odin

#endif // MESSAGE_FLAGS_HPP