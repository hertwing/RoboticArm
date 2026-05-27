#ifndef TYPES_HPP
#define TYPES_HPP

#include <cstddef>
#include <cstdint>

namespace odin
{
namespace odin_comm
{

using SequenceId = std::uint32_t;
using CorrelationId = std::uint32_t;
using PayloadSize = std::uint32_t;
using Checksum = std::uint32_t;

inline constexpr std::uint32_t ProtocolMagic = 0x4F44494E; // 'ODIN'
inline constexpr std::uint16_t ProtocolVersion = 1;

inline constexpr std::size_t DefaultMaxPayloadSize = 4096;

inline constexpr std::uint16_t EncodedMessageHeaderSize = 28;

} // namespace odin_comm
} // namespace odin

#endif // TYPES_HPP