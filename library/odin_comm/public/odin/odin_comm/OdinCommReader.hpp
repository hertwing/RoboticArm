#ifndef ODIN_COMM_READER_HPP
#define ODIN_COMM_READER_HPP

#include <chrono>
#include <exception>
#include <iostream>
#include <memory>
#include <utility>

#include "odin/odin_comm/config/CommunicationConfigRegistry.hpp"
#include "odin/odin_comm/core/Error.hpp"
#include "odin/odin_comm/core/Result.hpp"
#include "odin/odin_comm/messaging/Messenger.hpp"
#include "odin/odin_comm/messaging/Subscriber.hpp"
#include "odin/odin_comm/node/CurrentNodeProvider.hpp"
#include "odin/odin_comm/node/EndpointFactory.hpp"
#include "odin/odin_comm/transport/ITransport.hpp"
#include "odin/odin_comm/transport/TransportTraits.hpp"


namespace odin
{
namespace odin_comm
{

template <typename T, CommunicationTransport Transport>
class OdinCommReader
{
public:
    using ValueType = T;

    OdinCommReader()
        : OdinCommReader{createDefaultResourcesOrTerminate()}
    {
    }

    explicit OdinCommReader(std::unique_ptr<ITransport> transport)
        : m_transport{std::move(transport)}
        , m_messenger{*m_transport}
        , m_subscriber{m_messenger}
    {
    }

    OdinCommReader(const OdinCommReader&) = delete;
    OdinCommReader& operator=(const OdinCommReader&) = delete;

    OdinCommReader(OdinCommReader&&) = delete;
    OdinCommReader& operator=(OdinCommReader&&) = delete;

    [[nodiscard]] Result<ValueType> read(
        std::chrono::milliseconds timeout = std::chrono::milliseconds{1000}
    )
    {
        return m_subscriber.receive(timeout);
    }

private:
    explicit OdinCommReader(ReaderEndpointResources resources)
        : OdinCommReader{std::move(resources.transport)}
    {
    }

    [[nodiscard]] static ReaderEndpointResources createDefaultResourcesOrTerminate()
    {
        auto nodeResult = CurrentNodeProvider::detect();

        if (!nodeResult) {
            std::cerr << "Failed to detect ODIN_NODE: "
                      << toString(nodeResult.error())
                      << '\n';

            std::terminate();
        }

        auto registry = CommunicationConfigRegistry::defaultConfig();

        auto resourcesResult =
            EndpointFactory::createReaderResources<T, Transport>(
                nodeResult.value(),
                registry
            );

        if (!resourcesResult) {
            std::cerr << "Failed to create OdinCommReader resources: "
                      << toString(resourcesResult.error())
                      << '\n';

            std::terminate();
        }

        return std::move(resourcesResult.value());
    }

    std::unique_ptr<ITransport> m_transport;
    Messenger m_messenger;
    Subscriber<ValueType> m_subscriber;
};

} // namespace odin_comm
} // namespace odin

#endif // ODIN_COMM_READER_HPP