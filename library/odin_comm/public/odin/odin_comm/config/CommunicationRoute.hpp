#ifndef COMMUNICATION_ROUTE_HPP
#define COMMUNICATION_ROUTE_HPP

#include <cstdint>
#include <string>

#include "odin/odin_comm/config/EndpointRole.hpp"
#include "odin/odin_comm/config/MessageId.hpp"
#include "odin/odin_comm/config/NodeId.hpp"
#include "odin/odin_comm/config/ReliabilityKind.hpp"
#include "odin/odin_comm/config/TopicId.hpp"
#include "odin/odin_comm/config/TransportKind.hpp"

namespace odin
{
namespace odin_comm
{

struct CommunicationRoute
{
    NodeId node{NodeId::Unknown};
    EndpointRole role{EndpointRole::Unknown};

    MessageId messageId{MessageId::Unknown};
    TransportKind transport{TransportKind::Udp};

    TopicId topic{TopicId::Unknown};
    ReliabilityKind reliability{ReliabilityKind::BestEffort};

    std::uint16_t port{0};
    std::string shmemName{};
};

} // namespace odin_comm
} // namespace odin

#endif // COMMUNICATION_ROUTE_HPP