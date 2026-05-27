#ifndef SHMEM_TRANSPORT_HPP
#define SHMEM_TRANSPORT_HPP

#include <chrono>

#include "odin/odin_comm/core/Message.hpp"
#include "odin/odin_comm/core/Result.hpp"
#include "odin/odin_comm/core/Status.hpp"
#include "odin/odin_comm/shmem/ShmemTransportConfig.hpp"
#include "odin/odin_comm/transport/ITransport.hpp"

namespace odin
{
namespace odin_comm
{

class ShmemTransport final : public ITransport
{
public:
    explicit ShmemTransport(ShmemTransportConfig config);

    ~ShmemTransport() override;

    ShmemTransport(const ShmemTransport&) = delete;
    ShmemTransport& operator=(const ShmemTransport&) = delete;

    ShmemTransport(ShmemTransport&&) = delete;
    ShmemTransport& operator=(ShmemTransport&&) = delete;

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
    ShmemTransportConfig m_config;

    int m_fd{-1};
    void* m_memory{nullptr};
    std::size_t m_mappingSize{0};

    void* m_mutexSem{nullptr};
    void* m_dataReadySem{nullptr};
    void* m_slotFreeSem{nullptr};
};

} // namespace odin_comm
} // namespace odin

#endif // SHMEM_TRANSPORT_HPP