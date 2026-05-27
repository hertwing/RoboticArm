#ifndef COMMUNICATION_NODE_HPP
#define COMMUNICATION_NODE_HPP

#include <memory>
#include <utility>

#include "odin/odin_comm/OdinCommReader.hpp"
#include "odin/odin_comm/OdinCommWriter.hpp"
#include "odin/odin_comm/config/CommunicationConfigRegistry.hpp"
#include "odin/odin_comm/config/NodeId.hpp"
#include "odin/odin_comm/core/Result.hpp"
#include "odin/odin_comm/node/CurrentNodeProvider.hpp"
#include "odin/odin_comm/node/EndpointFactory.hpp"
#include "odin/odin_comm/transport/TransportTraits.hpp"


namespace odin
{
namespace odin_comm
{

class CommunicationNode
{
public:
    [[nodiscard]] static Result<CommunicationNode> fromEnvironment()
    {
        auto nodeResult = CurrentNodeProvider::detect();

        if (!nodeResult) {
            return Result<CommunicationNode>::failure(nodeResult.error());
        }

        return Result<CommunicationNode>::success(
            CommunicationNode{nodeResult.value()}
        );
    }

    [[nodiscard]] static Result<CommunicationNode> fromEnvironment(
        CommunicationConfigRegistry registry)
    {
        auto nodeResult = CurrentNodeProvider::detect();

        if (!nodeResult) {
            return Result<CommunicationNode>::failure(nodeResult.error());
        }

        return Result<CommunicationNode>::success(
            CommunicationNode{
                nodeResult.value(),
                std::move(registry)
            }
        );
    }

    explicit CommunicationNode(NodeId nodeId)
        : m_nodeId{nodeId}
        , m_registry{CommunicationConfigRegistry::defaultConfig()}
    {
    }

    CommunicationNode(
        NodeId nodeId,
        CommunicationConfigRegistry registry
    )
        : m_nodeId{nodeId}
        , m_registry{std::move(registry)}
    {
    }

    template <typename T, CommunicationTransport Transport>
    requires HasMessageTraits<T>
    [[nodiscard]] Result<std::unique_ptr<OdinCommWriter<T, Transport>>> createWriter() const
    {
        auto resourcesResult =
            EndpointFactory::createWriterResources<T, Transport>(
                m_nodeId,
                m_registry
            );

        if (!resourcesResult) {
            return Result<std::unique_ptr<OdinCommWriter<T, Transport>>>::failure(
                resourcesResult.error()
            );
        }

        auto resources = std::move(resourcesResult.value());

        auto writer = std::make_unique<OdinCommWriter<T, Transport>>(
            std::move(resources.transport),
            resources.reliability
        );

        return Result<std::unique_ptr<OdinCommWriter<T, Transport>>>::success(
            std::move(writer)
        );
    }

    template <typename T, CommunicationTransport Transport>
    requires HasMessageTraits<T>
    [[nodiscard]] Result<std::unique_ptr<OdinCommReader<T, Transport>>> createReader() const
    {
        auto resourcesResult =
            EndpointFactory::createReaderResources<T, Transport>(
                m_nodeId,
                m_registry
            );

        if (!resourcesResult) {
            return Result<std::unique_ptr<OdinCommReader<T, Transport>>>::failure(
                resourcesResult.error()
            );
        }

        auto resources = std::move(resourcesResult.value());

        auto reader = std::make_unique<OdinCommReader<T, Transport>>(
            std::move(resources.transport)
        );

        return Result<std::unique_ptr<OdinCommReader<T, Transport>>>::success(
            std::move(reader)
        );
    }

private:
    NodeId m_nodeId{NodeId::Unknown};
    CommunicationConfigRegistry m_registry;
};

} // namespace odin_comm
} // namespace odin

#endif // COMMUNICATION_NODE_HPP