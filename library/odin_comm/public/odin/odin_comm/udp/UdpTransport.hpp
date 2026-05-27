#ifndef UDP_TRANSPORT_HPP
#define UDP_TRANSPORT_HPP

#include <chrono>
#include <cstdint>

#include "odin/odin_comm/core/Message.hpp"
#include "odin/odin_comm/core/Result.hpp"
#include "odin/odin_comm/core/Status.hpp"
#include "odin/odin_comm/transport/ITransport.hpp"
#include "odin/odin_comm/udp/UdpTransportConfig.hpp"

namespace odin
{
namespace odin_comm
{

class UdpTransport final : public ITransport
{
public:
    explicit UdpTransport(UdpTransportConfig config);

    ~UdpTransport() override;

    UdpTransport(const UdpTransport&) = delete;
    UdpTransport& operator=(const UdpTransport&) = delete;

    UdpTransport(UdpTransport&&) = delete;
    UdpTransport& operator=(UdpTransport&&) = delete;

    [[nodiscard]] Status open() override;

    void close() noexcept override;

    [[nodiscard]] bool isOpen() const noexcept override;

    [[nodiscard]] Status send(
        const Message& message,
        std::chrono::milliseconds timeout
    ) override;

    [[nodiscard]] Result<Message> receive(
        std::chrono::milliseconds timeout
    ) override;

private:
    UdpTransportConfig m_config;
    int m_socketFd{-1};
};

} // namespace odin_comm
} // namespace odin

#endif // UDP_TRANSPORT_HPP