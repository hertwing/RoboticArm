#ifndef ITRANSPORT_HPP
#define ITRANSPORT_HPP

#include <chrono>

#include "odin/odin_comm/core/Message.hpp"
#include "odin/odin_comm/core/Result.hpp"
#include "odin/odin_comm/core/Status.hpp"

namespace odin
{
namespace odin_comm
{

class ITransport
{
public:
    virtual ~ITransport() = default;

    ITransport(const ITransport&) = delete;
    ITransport& operator=(const ITransport&) = delete;

    ITransport(ITransport&&) = delete;
    ITransport& operator=(ITransport&&) = delete;

    [[nodiscard]] virtual Status open() = 0;

    virtual void close() noexcept = 0;

    [[nodiscard]] virtual bool isOpen() const noexcept = 0;

    [[nodiscard]] virtual Status send(
        const Message& message,
        std::chrono::milliseconds timeout
    ) = 0;

    [[nodiscard]] virtual Result<Message> receive(
        std::chrono::milliseconds timeout
    ) = 0;

protected:
    ITransport() = default;
};

} // namespace odin_comm
} // namespace odin

#endif // ITRANSPORT_HPP