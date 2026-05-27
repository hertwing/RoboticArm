#include "odin/odin_comm/core/MessageCodec.hpp"

#include <cstdint>
#include <vector>

#include "odin/odin_comm/core/Checksum.hpp"
#include "odin/odin_comm/core/Error.hpp"
#include "odin/odin_comm/core/MessageFlags.hpp"
#include "odin/odin_comm/core/MessageHeader.hpp"
#include "odin/odin_comm/core/MessageType.hpp"
#include "odin/odin_comm/core/MessageValidation.hpp"
#include "odin/odin_comm/core/Types.hpp"

namespace odin
{
namespace odin_comm
{
namespace
{

constexpr std::size_t EncodedHeaderSize = EncodedMessageHeaderSize;

void appendUint16(std::vector<std::byte>& out, std::uint16_t value)
{
    out.push_back(static_cast<std::byte>((value >> 8) & 0xFF));
    out.push_back(static_cast<std::byte>(value & 0xFF));
}

void appendUint32(std::vector<std::byte>& out, std::uint32_t value)
{
    out.push_back(static_cast<std::byte>((value >> 24) & 0xFF));
    out.push_back(static_cast<std::byte>((value >> 16) & 0xFF));
    out.push_back(static_cast<std::byte>((value >> 8) & 0xFF));
    out.push_back(static_cast<std::byte>(value & 0xFF));
}

std::uint16_t readUint16(std::span<const std::byte> data, std::size_t offset)
{
    const auto b0 = static_cast<std::uint16_t>(data[offset]);
    const auto b1 = static_cast<std::uint16_t>(data[offset + 1]);

    return static_cast<std::uint16_t>((b0 << 8) | b1);
}

std::uint32_t readUint32(std::span<const std::byte> data, std::size_t offset)
{
    const auto b0 = static_cast<std::uint32_t>(data[offset]);
    const auto b1 = static_cast<std::uint32_t>(data[offset + 1]);
    const auto b2 = static_cast<std::uint32_t>(data[offset + 2]);
    const auto b3 = static_cast<std::uint32_t>(data[offset + 3]);

    return static_cast<std::uint32_t>(
        (b0 << 24) |
        (b1 << 16) |
        (b2 << 8) |
        b3
    );
}

} // namespace

std::vector<std::byte> encodeMessage(const Message& message)
{
    std::vector<std::byte> encoded;
    encoded.reserve(EncodedHeaderSize + message.payload.size());

    appendUint32(encoded, message.header.magic);
    appendUint16(encoded, message.header.version);
    appendUint16(encoded, static_cast<std::uint16_t>(EncodedHeaderSize));

    appendUint16(encoded, static_cast<std::uint16_t>(message.header.type));
    appendUint16(encoded, static_cast<std::uint16_t>(message.header.flags));

    appendUint32(encoded, message.header.sequence_id);
    appendUint32(encoded, message.header.correlation_id);

    appendUint32(encoded, message.header.payload_size);
    appendUint32(encoded, message.header.checksum);

    encoded.insert(
        encoded.end(),
        message.payload.begin(),
        message.payload.end()
    );

    return encoded;
}

Result<Message> decodeMessage(std::span<const std::byte> data)
{
    if (data.size() < EncodedHeaderSize) {
        return Result<Message>::failure(CommError::InvalidMessage);
    }

    Message message;

    message.header.magic = readUint32(data, 0);
    message.header.version = readUint16(data, 4);
    message.header.header_size = readUint16(data, 6);

    message.header.type = static_cast<MessageType>(readUint16(data, 8));
    message.header.flags = static_cast<MessageFlags>(readUint16(data, 10));

    message.header.sequence_id = readUint32(data, 12);
    message.header.correlation_id = readUint32(data, 16);

    message.header.payload_size = readUint32(data, 20);
    message.header.checksum = readUint32(data, 24);

    if (message.header.header_size != EncodedHeaderSize) {
        return Result<Message>::failure(CommError::InvalidMessage);
    }

    const auto expectedTotalSize =
        EncodedHeaderSize + static_cast<std::size_t>(message.header.payload_size);

    if (data.size() != expectedTotalSize) {
        return Result<Message>::failure(CommError::PayloadSizeMismatch);
    }

    if (message.header.payload_size > DefaultMaxPayloadSize) {
        return Result<Message>::failure(CommError::PayloadTooLarge);
    }

    message.payload.assign(
        data.begin() + static_cast<std::ptrdiff_t>(EncodedHeaderSize),
        data.end()
    );

    const auto validationStatus = validateMessage(message);

    if (!validationStatus) {
        return Result<Message>::failure(validationStatus.error());
    }

    return Result<Message>::success(std::move(message));
}

} // namespace odin_comm
} // namespace odin