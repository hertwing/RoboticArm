#include "odin/odin_comm/udp/UdpTransport.hpp"

#include "odin/odin_comm/core/MessageCodec.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <limits>
#include <utility>
#include <vector>


namespace odin
{
namespace odin_comm
{
namespace
{

[[nodiscard]] int toPollTimeout(std::chrono::milliseconds timeout) noexcept
{
    if (timeout.count() < 0) {
        return -1;
    }

    if (timeout.count() > static_cast<long long>(std::numeric_limits<int>::max())) {
        return std::numeric_limits<int>::max();
    }

    return static_cast<int>(timeout.count());
}

[[nodiscard]] sockaddr_in makeLocalAddress(std::uint16_t localPort, bool bindToAnyAddress)
{
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(localPort);
    address.sin_addr.s_addr = bindToAnyAddress ? htonl(INADDR_ANY) : htonl(INADDR_LOOPBACK);

    return address;
}

[[nodiscard]] bool makeRemoteAddress(
    const UdpEndpoint& endpoint,
    sockaddr_in& outAddress
)
{
    outAddress = {};
    outAddress.sin_family = AF_INET;
    outAddress.sin_port = htons(endpoint.port);

    const auto result = ::inet_pton(
        AF_INET,
        endpoint.address.c_str(),
        &outAddress.sin_addr
    );

    return result == 1;
}

} // namespace

UdpTransport::UdpTransport(UdpTransportConfig config)
    : m_config{std::move(config)}
{
}

UdpTransport::~UdpTransport()
{
    close();
}

Status UdpTransport::open()
{
    if (isOpen()) {
        return Status::failure(CommError::AlreadyOpen);
    }

    if (m_config.localPort == 0 ||
        m_config.remoteEndpoint.address.empty() ||
        m_config.remoteEndpoint.port == 0 ||
        m_config.maxDatagramSize < EncodedMessageHeaderSize) {
        return Status::failure(CommError::InvalidArgument);
    }

    m_socketFd = ::socket(AF_INET, SOCK_DGRAM, 0);

    if (m_socketFd < 0) {
        return Status::failure(CommError::TransportError);
    }

    const auto localAddress = makeLocalAddress(
        m_config.localPort,
        m_config.bindToAnyAddress
    );

    const auto bindResult = ::bind(
        m_socketFd,
        reinterpret_cast<const sockaddr*>(&localAddress),
        sizeof(localAddress)
    );

    if (bindResult < 0) {
        close();
        return Status::failure(CommError::TransportError);
    }

    sockaddr_in remoteAddress{};

    if (!makeRemoteAddress(m_config.remoteEndpoint, remoteAddress)) {
        close();
        return Status::failure(CommError::InvalidArgument);
    }

    const auto connectResult = ::connect(
        m_socketFd,
        reinterpret_cast<const sockaddr*>(&remoteAddress),
        sizeof(remoteAddress)
    );

    if (connectResult < 0) {
        close();
        return Status::failure(CommError::TransportError);
    }

    return Status::ok();
}

void UdpTransport::close() noexcept
{
    if (m_socketFd >= 0) {
        ::close(m_socketFd);
        m_socketFd = -1;
    }
}

bool UdpTransport::isOpen() const noexcept
{
    return m_socketFd >= 0;
}

Status UdpTransport::send(
    const Message& message,
    std::chrono::milliseconds timeout
)
{
    if (!isOpen()) {
        return Status::failure(CommError::NotOpen);
    }

    const auto encodedMessage = encodeMessage(message);

    if (encodedMessage.size() > m_config.maxDatagramSize) {
        return Status::failure(CommError::PayloadTooLarge);
    }

    pollfd descriptor{};
    descriptor.fd = m_socketFd;
    descriptor.events = POLLOUT;

    const auto pollResult = ::poll(
        &descriptor,
        1,
        toPollTimeout(timeout)
    );

    if (pollResult == 0) {
        return Status::failure(CommError::Timeout);
    }

    if (pollResult < 0) {
        return Status::failure(CommError::SendFailed);
    }

    const auto sentBytes = ::send(
        m_socketFd,
        encodedMessage.data(),
        encodedMessage.size(),
        0
    );

    if (sentBytes < 0) {
        return Status::failure(CommError::SendFailed);
    }

    if (static_cast<std::size_t>(sentBytes) != encodedMessage.size()) {
        return Status::failure(CommError::SendFailed);
    }

    return Status::ok();
}

Result<Message> UdpTransport::receive(
    std::chrono::milliseconds timeout
)
{
    if (!isOpen()) {
        return Result<Message>::failure(CommError::NotOpen);
    }

    pollfd descriptor{};
    descriptor.fd = m_socketFd;
    descriptor.events = POLLIN;

    const auto pollResult = ::poll(
        &descriptor,
        1,
        toPollTimeout(timeout)
    );

    if (pollResult == 0) {
        return Result<Message>::failure(CommError::Timeout);
    }

    if (pollResult < 0) {
        return Result<Message>::failure(CommError::ReceiveFailed);
    }

    std::vector<std::byte> buffer(m_config.maxDatagramSize);

    const auto receivedBytes = ::recv(
        m_socketFd,
        buffer.data(),
        buffer.size(),
        0
    );

    if (receivedBytes < 0) {
        return Result<Message>::failure(CommError::ReceiveFailed);
    }

    if (receivedBytes == 0) {
        return Result<Message>::failure(CommError::ReceiveFailed);
    }

    buffer.resize(static_cast<std::size_t>(receivedBytes));

    return decodeMessage(buffer);
}

} // namespace odin_comm
} // namespace odin