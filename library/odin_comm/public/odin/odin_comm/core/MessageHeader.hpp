#ifndef MESSAGE_HEADER_HPP
#define MESSAGE_HEADER_HPP

#include <cstdint>
#include <type_traits>

#include "odin/odin_comm/core/MessageFlags.hpp"
#include "odin/odin_comm/core/MessageType.hpp"
#include "odin/odin_comm/core/Types.hpp"

namespace odin
{
namespace odin_comm
{

struct MessageHeader
{
    std::uint32_t magic{ProtocolMagic};
    std::uint16_t version{ProtocolVersion};
    std::uint16_t header_size{EncodedMessageHeaderSize};

    MessageType type{MessageType::Unknown};
    MessageFlags flags{MessageFlags::None};

    SequenceId sequence_id{0};
    CorrelationId correlation_id{0};

    PayloadSize payload_size{0};
    Checksum checksum{0};
};

static_assert(std::is_trivially_copyable_v<MessageHeader>);
static_assert(std::is_standard_layout_v<MessageHeader>);

} // namespace odin_comm
} // namespace odin

#endif // MESSAGE_HEADER_HPP