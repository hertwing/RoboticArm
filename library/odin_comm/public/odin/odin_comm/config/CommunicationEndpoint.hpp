#ifndef COMMUNICATION_ENDPOINT_HPP
#define COMMUNICATION_ENDPOINT_HPP

#include <cstdint>
#include <string>

#include "odin/odin_comm/config/EndpointRole.hpp"
#include "odin/odin_comm/config/MessageId.hpp"
#include "odin/odin_comm/config/NodeId.hpp"
#include "odin/odin_comm/config/ReliabilityKind.hpp"

namespace odin
{
namespace odin_comm
{

struct UdpEndpointConfig
{
    EndpointRole role{EndpointRole::Unknown};

    MessageId messageId{MessageId::Unknown};

    NodeId peerNode{NodeId::Unknown};

    ReliabilityKind reliability{ReliabilityKind::BestEffort};

    std::uint16_t port{0};
};

struct ShmemEndpointConfig
{
    EndpointRole role{EndpointRole::Unknown};

    MessageId messageId{MessageId::Unknown};

    std::string shmemName;
};

} // namespace odin_comm
} // namespace odin

#endif // COMMUNICATION_ENDPOINT_HPP