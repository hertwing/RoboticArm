#ifndef RELIABILITY_KIND_HPP
#define RELIABILITY_KIND_HPP

#include <cstdint>

namespace odin
{
namespace odin_comm
{

enum class ReliabilityKind : std::uint8_t
{
    BestEffort = 0,
    Reliable = 1
};

} // namespace odin_comm
} // namespace odin

#endif // RELIABILITY_KIND_HPP