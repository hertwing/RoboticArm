#ifndef COMMUNICATION_CONFIG_REGISTRY_HPP
#define COMMUNICATION_CONFIG_REGISTRY_HPP

#include <utility>

#include "odin/odin_comm/config/CommunicationConfig.hpp"
#include "odin/odin_comm/config/CommunicationEndpoint.hpp"
#include "odin/odin_comm/config/EndpointRole.hpp"
#include "odin/odin_comm/config/MessageId.hpp"
#include "odin/odin_comm/core/Error.hpp"
#include "odin/odin_comm/core/Result.hpp"
#include "odin/odin_comm/shmem/ShmemChannelConfig.hpp"
#include "odin/odin_comm/shmem/ShmemTransportConfig.hpp"
#include "odin/odin_comm/udp/UdpEndpoint.hpp"
#include "odin/odin_comm/udp/UdpTransportConfig.hpp"


namespace odin
{
namespace odin_comm
{

class CommunicationConfigRegistry
{
public:
    CommunicationConfigRegistry()
        : m_config{makeDefaultCommunicationConfig()}
    {
    }

    explicit CommunicationConfigRegistry(CommunicationConfig config)
        : m_config{std::move(config)}
    {
    }

    [[nodiscard]] static CommunicationConfigRegistry defaultConfig()
    {
        return CommunicationConfigRegistry{makeDefaultCommunicationConfig()};
    }

    [[nodiscard]] Result<UdpEndpointConfig> findUdpEndpoint(
        NodeId localNode,
        MessageId messageId,
        EndpointRole role
    ) const
    {
        const auto* node = findNode(localNode);

        if (node == nullptr) {
            return Result<UdpEndpointConfig>::failure(CommError::NodeNotFound);
        }

        const UdpEndpointConfig* matchedEndpoint = nullptr;

        for (const auto& endpoint : node->udpEndpoints) {
            if (endpoint.messageId != messageId) {
                continue;
            }

            if (endpoint.role != role) {
                continue;
            }

            if (matchedEndpoint != nullptr) {
                return Result<UdpEndpointConfig>::failure(CommError::AmbiguousEndpoint);
            }

            matchedEndpoint = &endpoint;
        }

        if (matchedEndpoint == nullptr) {
            return Result<UdpEndpointConfig>::failure(CommError::EndpointNotFound);
        }

        return Result<UdpEndpointConfig>::success(*matchedEndpoint);
    }

    [[nodiscard]] Result<ShmemEndpointConfig> findShmemEndpoint(
        NodeId localNode,
        MessageId messageId,
        EndpointRole role
    ) const
    {
        const auto* node = findNode(localNode);

        if (node == nullptr) {
            return Result<ShmemEndpointConfig>::failure(CommError::NodeNotFound);
        }

        const ShmemEndpointConfig* matchedEndpoint = nullptr;

        for (const auto& endpoint : node->shmemEndpoints) {
            if (endpoint.messageId != messageId) {
                continue;
            }

            if (endpoint.role != role) {
                continue;
            }

            if (matchedEndpoint != nullptr) {
                return Result<ShmemEndpointConfig>::failure(CommError::AmbiguousEndpoint);
            }

            matchedEndpoint = &endpoint;
        }

        if (matchedEndpoint == nullptr) {
            return Result<ShmemEndpointConfig>::failure(CommError::EndpointNotFound);
        }

        return Result<ShmemEndpointConfig>::success(*matchedEndpoint);
    }

    [[nodiscard]] Result<ShmemTransportConfig> makeShmemWriterConfig(
        const ShmemEndpointConfig& endpoint) const
    {
        if (endpoint.role != EndpointRole::Writer) {
            return Result<ShmemTransportConfig>::failure(CommError::InvalidEndpointRole);
        }

        if (endpoint.shmemName.empty()) {
            return Result<ShmemTransportConfig>::failure(CommError::InvalidEndpointConfig);
        }

        return Result<ShmemTransportConfig>::success(
            makeShmemOwnerConfig(endpoint.shmemName)
        );
    }

    [[nodiscard]] Result<ShmemTransportConfig> makeShmemReaderConfig(
        const ShmemEndpointConfig& endpoint) const
    {
        if (endpoint.role != EndpointRole::Reader) {
            return Result<ShmemTransportConfig>::failure(CommError::InvalidEndpointRole);
        }

        if (endpoint.shmemName.empty()) {
            return Result<ShmemTransportConfig>::failure(CommError::InvalidEndpointConfig);
        }

        return Result<ShmemTransportConfig>::success(
            makeShmemClientConfig(endpoint.shmemName)
        );
    }

    [[nodiscard]] Result<UdpTransportConfig> makeUdpWriterConfig(
        NodeId localNode,
        const UdpEndpointConfig& endpoint
    ) const
    {
        if (endpoint.role != EndpointRole::Writer) {
            return Result<UdpTransportConfig>::failure(CommError::InvalidEndpointRole);
        }

        return makeUdpTransportConfig(localNode, endpoint);
    }

    [[nodiscard]] Result<UdpTransportConfig> makeUdpReaderConfig(
        NodeId localNode,
        const UdpEndpointConfig& endpoint) const
    {
        if (endpoint.role != EndpointRole::Reader) {
            return Result<UdpTransportConfig>::failure(CommError::InvalidEndpointRole);
        }

        return makeUdpTransportConfig(localNode, endpoint);
    }

private:
    [[nodiscard]] const NodeConfig* findNode(NodeId id) const noexcept
    {
        for (const auto& node : m_config.nodes) {
            if (node.id == id) {
                return &node;
            }
        }

        return nullptr;
    }

    [[nodiscard]] Result<UdpTransportConfig> makeUdpTransportConfig(
        NodeId localNode,
        const UdpEndpointConfig& endpoint
    ) const
    {
        if (endpoint.peerNode == NodeId::Unknown) {
            return Result<UdpTransportConfig>::failure(CommError::InvalidEndpointConfig);
        }

        if (endpoint.port == 0) {
            return Result<UdpTransportConfig>::failure(CommError::InvalidEndpointConfig);
        }

        if (endpoint.peerNode == localNode) {
            return Result<UdpTransportConfig>::failure(CommError::InvalidEndpointConfig);
        }

        const auto* peerNode = findNode(endpoint.peerNode);

        if (peerNode == nullptr) {
            return Result<UdpTransportConfig>::failure(CommError::PeerNodeNotFound);
        }

        UdpTransportConfig config{
            .localPort = endpoint.port,
            .remoteEndpoint = UdpEndpoint{
                .address = peerNode->ipAddress,
                .port = endpoint.port
            }
        };

        return Result<UdpTransportConfig>::success(std::move(config));
    }

    CommunicationConfig m_config;
};

} // namespace odin_comm
} // namespace odin

#endif // COMMUNICATION_CONFIG_REGISTRY_HPP