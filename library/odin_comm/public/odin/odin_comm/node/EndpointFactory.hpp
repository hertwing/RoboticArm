#ifndef ENDPOINT_FACTORY_HPP
#define ENDPOINT_FACTORY_HPP

#include <memory>
#include <utility>

#include "odin/odin_comm/config/CommunicationConfigRegistry.hpp"
#include "odin/odin_comm/config/CommunicationEndpoint.hpp"
#include "odin/odin_comm/config/EndpointRole.hpp"
#include "odin/odin_comm/config/NodeId.hpp"
#include "odin/odin_comm/config/ReliabilityKind.hpp"
#include "odin/odin_comm/config/TransportKind.hpp"
#include "odin/odin_comm/core/Error.hpp"
#include "odin/odin_comm/core/Result.hpp"
#include "odin/odin_comm/messaging/MessageTraits.hpp"
#include "odin/odin_comm/shmem/ShmemTransport.hpp"
#include "odin/odin_comm/transport/ITransport.hpp"
#include "odin/odin_comm/transport/TransportTraits.hpp"
#include "odin/odin_comm/udp/UdpTransport.hpp"

namespace odin
{
namespace odin_comm
{

struct WriterEndpointResources
{
    std::unique_ptr<ITransport> transport;
    ReliabilityKind reliability{ReliabilityKind::BestEffort};
};

struct ReaderEndpointResources
{
    std::unique_ptr<ITransport> transport;
};

class EndpointFactory
{
public:
    template <typename T, CommunicationTransport Transport>
    requires HasMessageTraits<T>
    [[nodiscard]] static Result<WriterEndpointResources> createWriterResources(
        NodeId nodeId,
        const CommunicationConfigRegistry& registry
    )
    {
        constexpr auto transportKind = TransportTraits<Transport>::transportKind;

        if constexpr (transportKind == TransportKind::Udp) {
            auto endpointResult = registry.findUdpEndpoint(
                nodeId,
                MessageTraits<T>::messageId,
                EndpointRole::Writer
            );

            if (!endpointResult) {
                return Result<WriterEndpointResources>::failure(
                    endpointResult.error()
                );
            }

            const auto endpoint = endpointResult.value();

            auto udpConfigResult = registry.makeUdpWriterConfig(
                nodeId,
                endpoint
            );

            if (!udpConfigResult) {
                return Result<WriterEndpointResources>::failure(
                    udpConfigResult.error()
                );
            }

            auto transport = std::make_unique<UdpTransport>(
                std::move(udpConfigResult.value())
            );

            const auto openStatus = transport->open();

            if (!openStatus) {
                return Result<WriterEndpointResources>::failure(openStatus.error());
            }

            WriterEndpointResources resources{
                .transport = std::move(transport),
                .reliability = endpoint.reliability
            };

            return Result<WriterEndpointResources>::success(std::move(resources));
        } else if constexpr (transportKind == TransportKind::Shmem) {
            auto endpointResult = registry.findShmemEndpoint(
                nodeId,
                MessageTraits<T>::messageId,
                EndpointRole::Writer
            );

            if (!endpointResult) {
                return Result<WriterEndpointResources>::failure(
                    endpointResult.error()
                );
            }

            const auto endpoint = endpointResult.value();

            auto shmemConfigResult = registry.makeShmemWriterConfig(endpoint);

            if (!shmemConfigResult) {
                return Result<WriterEndpointResources>::failure(
                    shmemConfigResult.error()
                );
            }

            auto transport = std::make_unique<ShmemTransport>(
                std::move(shmemConfigResult.value())
            );

            const auto openStatus = transport->open();

            if (!openStatus) {
                return Result<WriterEndpointResources>::failure(openStatus.error());
            }

            WriterEndpointResources resources{
                .transport = std::move(transport),
                .reliability = ReliabilityKind::BestEffort
            };

            return Result<WriterEndpointResources>::success(std::move(resources));
        } else {
            return Result<WriterEndpointResources>::failure(
                CommError::InvalidArgument
            );
        }
    }

    template <typename T, CommunicationTransport Transport>
    requires HasMessageTraits<T>
    [[nodiscard]] static Result<ReaderEndpointResources> createReaderResources(
        NodeId nodeId,
        const CommunicationConfigRegistry& registry
    )
    {
        constexpr auto transportKind = TransportTraits<Transport>::transportKind;

        if constexpr (transportKind == TransportKind::Udp) {
            auto endpointResult = registry.findUdpEndpoint(
                nodeId,
                MessageTraits<T>::messageId,
                EndpointRole::Reader
            );

            if (!endpointResult) {
                return Result<ReaderEndpointResources>::failure(
                    endpointResult.error()
                );
            }

            const auto endpoint = endpointResult.value();

            auto udpConfigResult = registry.makeUdpReaderConfig(
                nodeId,
                endpoint
            );

            if (!udpConfigResult) {
                return Result<ReaderEndpointResources>::failure(
                    udpConfigResult.error()
                );
            }

            auto transport = std::make_unique<UdpTransport>(
                std::move(udpConfigResult.value())
            );

            const auto openStatus = transport->open();

            if (!openStatus) {
                return Result<ReaderEndpointResources>::failure(openStatus.error());
            }

            ReaderEndpointResources resources{
                .transport = std::move(transport)
            };

            return Result<ReaderEndpointResources>::success(std::move(resources));
        } else if constexpr (transportKind == TransportKind::Shmem) {
            auto endpointResult = registry.findShmemEndpoint(
                nodeId,
                MessageTraits<T>::messageId,
                EndpointRole::Reader
            );

            if (!endpointResult) {
                return Result<ReaderEndpointResources>::failure(
                    endpointResult.error()
                );
            }

            const auto endpoint = endpointResult.value();

            auto shmemConfigResult = registry.makeShmemReaderConfig(endpoint);

            if (!shmemConfigResult) {
                return Result<ReaderEndpointResources>::failure(
                    shmemConfigResult.error()
                );
            }

            auto transport = std::make_unique<ShmemTransport>(
                std::move(shmemConfigResult.value())
            );

            const auto openStatus = transport->open();

            if (!openStatus) {
                return Result<ReaderEndpointResources>::failure(openStatus.error());
            }

            ReaderEndpointResources resources{
                .transport = std::move(transport)
            };

            return Result<ReaderEndpointResources>::success(std::move(resources));
        } else {
            return Result<ReaderEndpointResources>::failure(
                CommError::InvalidArgument
            );
        }
    }
};

} // namespace odin_comm
} // namespace odin

#endif // ENDPOINT_FACTORY_HPP