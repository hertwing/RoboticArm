#ifndef MESSAGE_VALIDATION_HPP
#define MESSAGE_VALIDATION_HPP

#include "odin/odin_comm/core/Message.hpp"
#include "odin/odin_comm/core/Status.hpp"

namespace odin
{
namespace odin_comm
{

[[nodiscard]] Status validateMessage(const Message& message) noexcept;

} // namespace odin_comm
} // namespace odin

#endif // MESSAGE_VALIDATION_HPP