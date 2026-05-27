#ifndef SHMEM_TRANSPORT_CONFIG_HPP
#define SHMEM_TRANSPORT_CONFIG_HPP

#include <cstddef>
#include <string>

#include "odin/odin_comm/core/Types.hpp"

namespace odin
{
namespace odin_comm
{

struct ShmemTransportConfig
{
    std::string name;
    std::size_t maxPayloadSize{DefaultMaxPayloadSize};

    bool create{false};
    bool unlinkOnClose{false};

    /*
     * Single-slot transport:
     * - one writer
     * - one reader
     * - one message stored at a time
     */
};

} // namespace odin_comm
} // namespace odin

#endif // SHMEM_TRANSPORT_CONFIG_HPP