#include "odin/odin_comm/shmem/ShmemDuplexTransport.hpp"

#include <utility>

namespace odin
{
namespace odin_comm
{

ShmemDuplexTransport::ShmemDuplexTransport(ShmemDuplexTransportConfig config)
    : m_outgoing{std::move(config.outgoing)}
    , m_incoming{std::move(config.incoming)}
{
}

ShmemDuplexTransport::~ShmemDuplexTransport()
{
    close();
}

Status ShmemDuplexTransport::open()
{
    auto status = m_outgoing.open();

    if (!status) {
        return status;
    }

    status = m_incoming.open();

    if (!status) {
        m_outgoing.close();
        return status;
    }

    return Status::ok();
}

void ShmemDuplexTransport::close() noexcept
{
    m_incoming.close();
    m_outgoing.close();
}

bool ShmemDuplexTransport::isOpen() const noexcept
{
    return m_outgoing.isOpen() && m_incoming.isOpen();
}

Status ShmemDuplexTransport::send(
    const Message& message,
    std::chrono::milliseconds timeout
)
{
    return m_outgoing.send(message, timeout);
}

Result<Message> ShmemDuplexTransport::receive(
    std::chrono::milliseconds timeout
)
{
    return m_incoming.receive(timeout);
}

} // namespace odin_comm
} // namespace odin