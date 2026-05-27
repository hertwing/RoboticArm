#ifndef UDP_ENDPOINT_HPP
#define UDP_ENDPOINT_HPP

#include <cstdint>
#include <string>

namespace odin
{
namespace odin_comm
{

struct UdpEndpoint
{
    std::string address;
    std::uint16_t port{0};
};

} // namespace odin_comm
} // namespace odin

#endif // UDP_ENDPOINT_HPP