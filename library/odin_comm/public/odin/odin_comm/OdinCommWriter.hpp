#ifndef ODIN_COMM_WRITER_HPP
#define ODIN_COMM_WRITER_HPP

#include <chrono>
#include <exception>
#include <iostream>
#include <memory>
#include <utility>

#include "odin/odin_comm/config/CommunicationConfigRegistry.hpp"
#include "odin/odin_comm/config/ReliabilityKind.hpp"
#include "odin/odin_comm/core/Error.hpp"
#include "odin/odin_comm/core/Status.hpp"
#include "odin/odin_comm/messaging/Messenger.hpp"
#include "odin/odin_comm/messaging/Publisher.hpp"
#include "odin/odin_comm/messaging/RetryPolicy.hpp"
#include "odin/odin_comm/node/CurrentNodeProvider.hpp"
#include "odin/odin_comm/node/EndpointFactory.hpp"
#include "odin/odin_comm/transport/ITransport.hpp"
#include "odin/odin_comm/transport/TransportTraits.hpp"

namespace odin
{
namespace odin_comm
{

template <typename T, CommunicationTransport Transport>
class OdinCommWriter
{
public:
    using ValueType = T;

    OdinCommWriter() : OdinCommWriter{createDefaultResourcesOrTerminate()}
    {
    }

    OdinCommWriter(
        std::unique_ptr<ITransport> transport,
        ReliabilityKind reliability
    )
        : m_transport{std::move(transport)}
        , m_messenger{*m_transport}
        , m_publisher{m_messenger}
        , m_reliability{reliability}
    {
    }

    OdinCommWriter(const OdinCommWriter&) = delete;
    OdinCommWriter& operator=(const OdinCommWriter&) = delete;

    OdinCommWriter(OdinCommWriter&&) = delete;
    OdinCommWriter& operator=(OdinCommWriter&&) = delete;

    [[nodiscard]] Status write(
        const ValueType& value,
        std::chrono::milliseconds timeout = std::chrono::milliseconds{1000}
    )
    {
        switch (m_reliability)
        {
            case ReliabilityKind::BestEffort:
                return m_publisher.publish(value, timeout);

            case ReliabilityKind::Reliable:
                return m_publisher.publishReliable(value, timeout, m_retryPolicy);

            default:
                return Status::failure(CommError::InvalidArgument);
        }
    }

    void setRetryPolicy(RetryPolicy retryPolicy) noexcept
    {
        m_retryPolicy = retryPolicy;
    }

private:
    explicit OdinCommWriter(WriterEndpointResources resources)
        : OdinCommWriter{
            std::move(resources.transport),
            resources.reliability}
    {
    }

    [[nodiscard]] static WriterEndpointResources createDefaultResourcesOrTerminate()
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
            EndpointFactory::createWriterResources<T, Transport>(
                nodeResult.value(),
                registry
            );

        if (!resourcesResult) {
            std::cerr << "Failed to create OdinCommWriter resources: "
                      << toString(resourcesResult.error())
                      << '\n';

            std::terminate();
        }

        return std::move(resourcesResult.value());
    }

    std::unique_ptr<ITransport> m_transport;
    Messenger m_messenger;
    Publisher<ValueType> m_publisher;

    ReliabilityKind m_reliability{ReliabilityKind::BestEffort};

    RetryPolicy m_retryPolicy{
        .maxAttempts = 3,
        .ackTimeout = std::chrono::milliseconds{100}
    };
};

} // namespace odin_comm
} // namespace odin

#endif // ODIN_COMM_WRITER_HPP