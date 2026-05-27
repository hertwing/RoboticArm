#ifndef SHMEM_CHANNEL_CONFIG_HPP
#define SHMEM_CHANNEL_CONFIG_HPP

#include <cstddef>
#include <string>
#include <string_view>

#include "odin/odin_comm/core/Types.hpp"
#include "odin/odin_comm/shmem/ShmemDuplexTransportConfig.hpp"
#include "odin/odin_comm/shmem/ShmemTransportConfig.hpp"

namespace odin
{
namespace odin_comm
{

[[nodiscard]] inline ShmemTransportConfig makeShmemOwnerConfig(
    std::string_view name,
    std::size_t maxPayloadSize = DefaultMaxPayloadSize
)
{
    return ShmemTransportConfig{
        .name = std::string{name},
        .maxPayloadSize = maxPayloadSize,
        .create = true,
        .unlinkOnClose = true
    };
}

[[nodiscard]] inline ShmemTransportConfig makeShmemClientConfig(
    std::string_view name,
    std::size_t maxPayloadSize = DefaultMaxPayloadSize
)
{
    return ShmemTransportConfig{
        .name = std::string{name},
        .maxPayloadSize = maxPayloadSize,
        .create = false,
        .unlinkOnClose = false
    };
}

[[nodiscard]] inline ShmemDuplexTransportConfig makeShmemDuplexOwnerConfig(
    std::string_view dataChannelName,
    std::string_view ackChannelName,
    std::size_t maxPayloadSize = DefaultMaxPayloadSize
)
{
    return ShmemDuplexTransportConfig{
        .outgoing = makeShmemOwnerConfig(
            ackChannelName,
            maxPayloadSize
        ),
        .incoming = makeShmemOwnerConfig(
            dataChannelName,
            maxPayloadSize
        )
    };
}

[[nodiscard]] inline ShmemDuplexTransportConfig makeShmemDuplexClientConfig(
    std::string_view dataChannelName,
    std::string_view ackChannelName,
    std::size_t maxPayloadSize = DefaultMaxPayloadSize
)
{
    return ShmemDuplexTransportConfig{
        .outgoing = makeShmemClientConfig(
            dataChannelName,
            maxPayloadSize
        ),
        .incoming = makeShmemClientConfig(
            ackChannelName,
            maxPayloadSize
        )
    };
}

} // namespace odin_comm
} // namespace odin

#endif // SHMEM_CHANNEL_CONFIG_HPP