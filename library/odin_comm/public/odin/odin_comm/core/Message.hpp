#ifndef MESSAGE_HPP
#define MESSAGE_HPP

#include <cstddef>
#include <vector>

#include "odin/odin_comm/core/MessageHeader.hpp"

namespace odin
{
namespace odin_comm
{

struct Message {
    MessageHeader header{};
    std::vector<std::byte> payload{};
};

} // namespace odin_comm
} // namespace odin

#endif // MESSAGE_HPP