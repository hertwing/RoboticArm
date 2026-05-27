#ifndef SHMEM_DUPLEX_TRANSPORT_HPP
#define SHMEM_DUPLEX_TRANSPORT_HPP

#include <chrono>

#include "odin/odin_comm/core/Message.hpp"
#include "odin/odin_comm/core/Result.hpp"
#include "odin/odin_comm/core/Status.hpp"
#include "odin/odin_comm/shmem/ShmemDuplexTransportConfig.hpp"
#include "odin/odin_comm/shmem/ShmemTransport.hpp"
#include "odin/odin_comm/transport/ITransport.hpp"

namespace odin
{
namespace odin_comm
{

class ShmemDuplexTransport final : public ITransport
{
public:
    explicit ShmemDuplexTransport(ShmemDuplexTransportConfig config);

    ~ShmemDuplexTransport() override;

    ShmemDuplexTransport(const ShmemDuplexTransport&) = delete;
    ShmemDuplexTransport& operator=(const ShmemDuplexTransport&) = delete;

    ShmemDuplexTransport(ShmemDuplexTransport&&) = delete;
    ShmemDuplexTransport& operator=(ShmemDuplexTransport&&) = delete;

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
    ShmemTransport m_outgoing;
    ShmemTransport m_incoming;
};

} // namespace odin_comm
} // namespace odin

#endif // SHMEM_DUPLEX_TRANSPORT_HPP