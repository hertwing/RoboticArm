#ifndef SERIALIZER_HPP
#define SERIALIZER_HPP

#include <bit>
#include <cstddef>
#include <cstring>
#include <optional>
#include <span>
#include <type_traits>
#include <vector>

namespace odin
{
namespace odin_comm
{

template <typename T>
concept TriviallySerializable =
    std::is_trivially_copyable_v<T> &&
    std::is_standard_layout_v<T>;

template <TriviallySerializable T>
[[nodiscard]] std::vector<std::byte> serializeTrivial(const T& value)
{
    std::vector<std::byte> bytes(sizeof(T));
    std::memcpy(bytes.data(), &value, sizeof(T));
    return bytes;
}

template <TriviallySerializable T>
[[nodiscard]] std::optional<T> deserializeTrivial(std::span<const std::byte> bytes)
{
    if (bytes.size() != sizeof(T))
    {
        return std::nullopt;
    }

    T value{};
    std::memcpy(&value, bytes.data(), sizeof(T));
    return value;
}

} // namespace odin_comm
} // namespace odin

#endif // SERIALIZER_HPP