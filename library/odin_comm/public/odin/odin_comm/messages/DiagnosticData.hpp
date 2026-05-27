#ifndef DIAGNOSTIC_DATA_HPP
#define DIAGNOSTIC_DATA_HPP

#include <cstdint>

#include "odin/odin_comm/config/MessageId.hpp"
#include "odin/odin_comm/core/MessageType.hpp"
#include "odin/odin_comm/messaging/MessageTraits.hpp"

namespace odin
{
namespace odin_comm
{

struct DiagnosticData
{
    std::uint32_t cpu_usage{};
    std::uint32_t ram_usage{};
    std::uint32_t cpu_temp{};
    double latency{};

    bool operator!=(const DiagnosticData & other) const
    {
        return cpu_usage != other.cpu_usage || ram_usage != other.ram_usage || cpu_temp != other.cpu_temp || latency != other.latency;
    }
};

template <>
struct MessageTraits<DiagnosticData>
{
    static constexpr MessageType messageType = MessageType::DiagnosticData;
    static constexpr MessageId messageId = MessageId::DiagnosticData;
};

} // namespace odin_comm
} // namespace odin

#endif // DIAGNOSTIC_DATA_HPP