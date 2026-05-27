#include "odin/odin_comm/core/MessageFactory.hpp"

#include "odin/odin_comm/core/Checksum.hpp"
#include "odin/odin_comm/core/Types.hpp"

#include <vector>


namespace odin
{
namespace odin_comm
{

Result<Message> createMessage(
    MessageType type,
    MessageFlags flags,
    SequenceId sequenceId,
    CorrelationId correlationId,
    std::span<const std::byte> payload)
{
    if (payload.size() > DefaultMaxPayloadSize)
    {
        return Result<Message>::failure(CommError::PayloadTooLarge);
    }

    Message message;
    message.payload.assign(payload.begin(), payload.end());

    message.header.magic = ProtocolMagic;
    message.header.version = ProtocolVersion;
    message.header.header_size = EncodedMessageHeaderSize;
    message.header.type = type;
    message.header.flags = flags;
    message.header.sequence_id = sequenceId;
    message.header.correlation_id = correlationId;
    message.header.payload_size = static_cast<PayloadSize>(message.payload.size());
    message.header.checksum = calculateChecksum(message.payload);

    return Result<Message>::success(std::move(message));
}

Message createAckMessage(
    SequenceId sequenceId,
    CorrelationId acknowledgedSequenceId)
{
    Message message;

    message.header.magic = ProtocolMagic;
    message.header.version = ProtocolVersion;
    message.header.header_size = EncodedMessageHeaderSize;
    message.header.type = MessageType::Ack;
    message.header.flags = MessageFlags::IsAck;
    message.header.sequence_id = sequenceId;
    message.header.correlation_id = acknowledgedSequenceId;
    message.header.payload_size = 0;
    message.header.checksum = calculateChecksum(message.payload);

    return message;
}

} // namespace odin_comm
} // namespace odin