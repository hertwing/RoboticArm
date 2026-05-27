#ifndef NODE_ID_HPP
#define NODE_ID_HPP

#include <cstdint>

namespace odin
{
namespace odin_comm
{

enum class NodeId : std::uint16_t
{
    Unknown = 0,
    Gui = 1,
    Arm = 2,
    Vision = 3,
    Diagnostics = 4
};

} // namespace odin_comm
} // namespace odin

#endif // NODE_ID_HPP