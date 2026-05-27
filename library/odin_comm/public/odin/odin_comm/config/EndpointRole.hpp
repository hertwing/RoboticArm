#ifndef ENDPOINT_ROLE_HPP
#define ENDPOINT_ROLE_HPP

#include <cstdint>

namespace odin
{
namespace odin_comm
{

enum class EndpointRole : std::uint8_t
{
    Unknown = 0,
    Writer,
    Reader,
};

} // namespace odin_comm
} // namespace odin

#endif // ENDPOINT_ROLE_HPP