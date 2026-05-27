#ifndef SHMEM_DUPLEX_TRANSPORT_CONFIG_HPP
#define SHMEM_DUPLEX_TRANSPORT_CONFIG_HPP

#include "odin/odin_comm/shmem/ShmemTransportConfig.hpp"

namespace odin
{
namespace odin_comm
{

struct ShmemDuplexTransportConfig
{
    ShmemTransportConfig outgoing;
    ShmemTransportConfig incoming;
};

} // namespace odin_comm
} // namespace odin

#endif // SHMEM_DUPLEX_TRANSPORT_CONFIG_HPP