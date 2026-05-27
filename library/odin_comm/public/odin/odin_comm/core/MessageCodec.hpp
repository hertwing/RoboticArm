#ifndef MESSAGE_CODEC_HPP
#define MESSAGE_CODEC_HPP

#include <cstddef>
#include <span>
#include <vector>

#include "odin/odin_comm/core/Message.hpp"
#include "odin/odin_comm/core/Result.hpp"

namespace odin
{
namespace odin_comm
{

[[nodiscard]] std::vector<std::byte> encodeMessage(const Message& message);

[[nodiscard]] Result<Message> decodeMessage(std::span<const std::byte> data);

} // namespace odin_comm
} // namespace odin

#endif // MESSAGE_CODEC_HPP