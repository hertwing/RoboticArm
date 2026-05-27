#ifndef MESSAGE_TRAITS_HPP
#define MESSAGE_TRAITS_HPP

#include <concepts>

#include "odin/odin_comm/config/MessageId.hpp"
#include "odin/odin_comm/core/MessageType.hpp"

namespace odin
{
namespace odin_comm
{

template <typename T>
struct MessageTraits;

template <typename T>
concept HasMessageTraits = requires
{
    { MessageTraits<T>::messageType } -> std::convertible_to<MessageType>;
    { MessageTraits<T>::messageId } -> std::convertible_to<MessageId>;
};

} // namespace odin_comm
} // namespace odin

#endif // MESSAGE_TRAITS_HPP