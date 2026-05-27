#include "odin/odin_comm/core/MessageValidation.hpp"

#include "odin/odin_comm/core/Checksum.hpp"
#include "odin/odin_comm/core/Types.hpp"

namespace odin
{
namespace odin_comm
{

Status validateMessage(const Message& message) noexcept
{
    const auto& header = message.header;

    if (header.magic != ProtocolMagic) {
        return Status::failure(CommError::InvalidMagic);
    }

    if (header.version != ProtocolVersion) {
        return Status::failure(CommError::UnsupportedVersion);
    }

    if (header.header_size != EncodedMessageHeaderSize) {
        return Status::failure(CommError::InvalidMessage);
    }

    if (header.payload_size != message.payload.size()) {
        return Status::failure(CommError::PayloadSizeMismatch);
    }

    const auto checksum = calculateChecksum(message.payload);

    if (header.checksum != checksum) {
        return Status::failure(CommError::ChecksumMismatch);
    }

    return Status::ok();
}

} // namespace odin_comm
} // namespace odin