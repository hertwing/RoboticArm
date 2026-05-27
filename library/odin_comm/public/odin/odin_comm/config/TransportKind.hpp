#ifndef TRANSPORT_KIND_HPP
#define TRANSPORT_KIND_HPP

#include <cstdint>

namespace odin
{
namespace odin_comm
{

enum class TransportKind : std::uint8_t
{
    Udp = 0,
    Shmem = 1
};

} // namespace odin_comm
} // namespace odin

#endif // TRANSPORT_KIND_HPP