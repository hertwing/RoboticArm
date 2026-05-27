#ifndef CHECKSUM_HPP
#define CHECKSUM_HPP

#include <cstddef>
#include <span>

#include "odin/odin_comm/core/Types.hpp"

namespace odin
{
namespace odin_comm
{

[[nodiscard]] Checksum calculateChecksum(std::span<const std::byte> data) noexcept;

} // namespace odin_comm
} // namespace odin

#endif // CHECKSUM_HPP