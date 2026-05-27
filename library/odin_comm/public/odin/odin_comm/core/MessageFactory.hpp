#ifndef MESSAGE_FACTORY_HPP
#define MESSAGE_FACTORY_HPP

#include <span>

#include "odin/odin_comm/core/Message.hpp"
#include "odin/odin_comm/core/Result.hpp"

namespace odin
{
namespace odin_comm
{

[[nodiscard]] Result<Message> createMessage(
    MessageType type,
    MessageFlags flags,
    SequenceId sequenceId,
    CorrelationId correlationId,
    std::span<const std::byte> payload
);

[[nodiscard]] Message createAckMessage(
    SequenceId sequenceId,
    CorrelationId acknowledgedSequenceId
);

} // namespace odin_comm
} // namespace odin

#endif // MESSAGE_FACTORY_HPP