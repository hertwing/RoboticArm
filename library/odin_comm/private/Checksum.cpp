#include "odin/odin_comm/core/Checksum.hpp"

#include <cstdint>

namespace odin
{
namespace odin_comm
{

Checksum calculateChecksum(std::span<const std::byte> data) noexcept
{
    constexpr std::uint32_t fnv_offset_basis = 2166136261u;
    constexpr std::uint32_t fnv_prime = 16777619u;

    std::uint32_t hash = fnv_offset_basis;

    for (const auto byte : data)
    {
        hash ^= static_cast<std::uint32_t>(byte);
        hash *= fnv_prime;
    }

    return hash;
}

} // namespace odin_comm
} // namespace odin