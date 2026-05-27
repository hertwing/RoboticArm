#include "odin/odin_comm/shmem/ShmemTransport.hpp"

#include "odin/odin_comm/core/MessageCodec.hpp"

#include <fcntl.h>
#include <semaphore.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <limits>
#include <string>
#include <utility>
#include <vector>


namespace odin
{
namespace odin_comm
{
namespace
{

constexpr std::uint32_t ShmemMagic = 0x53484D45; // 'SHME'

struct ShmemHeader
{
    std::uint32_t magic{ShmemMagic};
    std::uint32_t capacity{0};
    std::uint32_t size{0};
};

[[nodiscard]] std::string makeSemaphoreName(
    const std::string& baseName,
    const char* suffix
)
{
    return baseName + suffix;
}

[[nodiscard]] int toSemTimeoutMs(std::chrono::milliseconds timeout) noexcept
{
    if (timeout.count() < 0) {
        return -1;
    }

    if (timeout.count() > static_cast<long long>(std::numeric_limits<int>::max())) {
        return std::numeric_limits<int>::max();
    }

    return static_cast<int>(timeout.count());
}

[[nodiscard]] timespec makeAbsTimeout(std::chrono::milliseconds timeout)
{
    timespec ts{};

    ::clock_gettime(CLOCK_REALTIME, &ts);

    const auto seconds = timeout.count() / 1000;
    const auto milliseconds = timeout.count() % 1000;

    ts.tv_sec += seconds;
    ts.tv_nsec += milliseconds * 1000000;

    if (ts.tv_nsec >= 1000000000L) {
        ts.tv_sec += 1;
        ts.tv_nsec -= 1000000000L;
    }

    return ts;
}

[[nodiscard]] Status waitSemaphore(
    sem_t* semaphore,
    std::chrono::milliseconds timeout
)
{
    if (timeout.count() < 0) {
        while (::sem_wait(semaphore) == -1) {
            if (errno != EINTR) {
                return Status::failure(CommError::TransportError);
            }
        }

        return Status::ok();
    }

    const auto absTimeout = makeAbsTimeout(timeout);

    while (::sem_timedwait(semaphore, const_cast<timespec*>(&absTimeout)) == -1) {
        if (errno == ETIMEDOUT) {
            return Status::failure(CommError::Timeout);
        }

        if (errno != EINTR) {
            return Status::failure(CommError::TransportError);
        }
    }

    return Status::ok();
}

[[nodiscard]] Status postSemaphore(sem_t* semaphore)
{
    if (::sem_post(semaphore) == -1) {
        return Status::failure(CommError::TransportError);
    }

    return Status::ok();
}

} // namespace

ShmemTransport::ShmemTransport(ShmemTransportConfig config)
    : m_config{std::move(config)}
{
}

ShmemTransport::~ShmemTransport()
{
    close();
}

Status ShmemTransport::open()
{
    if (isOpen()) {
        return Status::failure(CommError::AlreadyOpen);
    }

    if (m_config.name.empty() || m_config.maxPayloadSize == 0) {
        return Status::failure(CommError::InvalidArgument);
    }

    const auto capacity =
        static_cast<std::uint32_t>(EncodedMessageHeaderSize + m_config.maxPayloadSize);

    m_mappingSize = sizeof(ShmemHeader) + capacity;

    const int flags = m_config.create ? (O_CREAT | O_RDWR) : O_RDWR;

    m_fd = ::shm_open(
        m_config.name.c_str(),
        flags,
        0666
    );
    if (m_fd < 0) {
        close();
        return Status::failure(CommError::TransportError);
    }

    if (m_config.create) {
        if (::ftruncate(m_fd, static_cast<off_t>(m_mappingSize)) == -1) {
            close();
            return Status::failure(CommError::TransportError);
        }
    }

    m_memory = ::mmap(
        nullptr,
        m_mappingSize,
        PROT_READ | PROT_WRITE,
        MAP_SHARED,
        m_fd,
        0
    );

    if (m_memory == MAP_FAILED) {
        m_memory = nullptr;
        close();
        return Status::failure(CommError::TransportError);
    }

    auto* header = static_cast<ShmemHeader*>(m_memory);

    if (m_config.create) {
        header->magic = ShmemMagic;
        header->capacity = capacity;
        header->size = 0;
    } else {
        if (header->magic != ShmemMagic || header->capacity != capacity) {
            close();
            return Status::failure(CommError::InvalidMessage);
        }
    }

    const auto mutexName = makeSemaphoreName(m_config.name, "_mutex");
    const auto dataReadyName = makeSemaphoreName(m_config.name, "_data_ready");
    const auto slotFreeName = makeSemaphoreName(m_config.name, "_slot_free");

    const int semFlags = m_config.create ? O_CREAT : 0;

    m_mutexSem = ::sem_open(mutexName.c_str(), semFlags, 0666, 1);
    m_dataReadySem = ::sem_open(dataReadyName.c_str(), semFlags, 0666, 0);
    m_slotFreeSem = ::sem_open(slotFreeName.c_str(), semFlags, 0666, 1);

    if (m_mutexSem == SEM_FAILED ||
        m_dataReadySem == SEM_FAILED ||
        m_slotFreeSem == SEM_FAILED) {
        close();
        return Status::failure(CommError::TransportError);
    }

    return Status::ok();
}

void ShmemTransport::close() noexcept
{
    if (m_memory != nullptr) {
        ::munmap(m_memory, m_mappingSize);
        m_memory = nullptr;
        m_mappingSize = 0;
    }

    if (m_fd >= 0) {
        ::close(m_fd);
        m_fd = -1;
    }

    if (m_mutexSem != nullptr && m_mutexSem != SEM_FAILED) {
        ::sem_close(static_cast<sem_t*>(m_mutexSem));
        m_mutexSem = nullptr;
    }

    if (m_dataReadySem != nullptr && m_dataReadySem != SEM_FAILED) {
        ::sem_close(static_cast<sem_t*>(m_dataReadySem));
        m_dataReadySem = nullptr;
    }

    if (m_slotFreeSem != nullptr && m_slotFreeSem != SEM_FAILED) {
        ::sem_close(static_cast<sem_t*>(m_slotFreeSem));
        m_slotFreeSem = nullptr;
    }

    if (m_config.unlinkOnClose) {
        const auto mutexName = makeSemaphoreName(m_config.name, "_mutex");
        const auto dataReadyName = makeSemaphoreName(m_config.name, "_data_ready");
        const auto slotFreeName = makeSemaphoreName(m_config.name, "_slot_free");

        ::sem_unlink(mutexName.c_str());
        ::sem_unlink(dataReadyName.c_str());
        ::sem_unlink(slotFreeName.c_str());
        ::shm_unlink(m_config.name.c_str());
    }
}

bool ShmemTransport::isOpen() const noexcept
{
    return m_fd >= 0 && m_memory != nullptr;
}

// St

Status ShmemTransport::send(
    const Message& message,
    std::chrono::milliseconds timeout
)
{
    if (!isOpen()) {
        return Status::failure(CommError::NotOpen);
    }

    const auto encodedMessage = encodeMessage(message);

    auto* header = static_cast<ShmemHeader*>(m_memory);

    if (encodedMessage.size() > header->capacity) {
        return Status::failure(CommError::PayloadTooLarge);
    }

    auto* mutex = static_cast<sem_t*>(m_mutexSem);

    auto status = waitSemaphore(mutex, timeout);

    if (!status) {
        return status;
    }

    auto* payloadMemory = static_cast<std::byte*>(m_memory) + sizeof(ShmemHeader);

    std::memcpy(
        payloadMemory,
        encodedMessage.data(),
        encodedMessage.size()
    );

    header->size = static_cast<std::uint32_t>(encodedMessage.size());

    return postSemaphore(mutex);
}

// Result<Message> ShmemTransport::receive(
//     std::chrono::milliseconds timeout
// )
// {
//     if (!isOpen()) {
//         return Result<Message>::failure(CommError::NotOpen);
//     }

//     auto* mutex = static_cast<sem_t*>(m_mutexSem);
//     auto* dataReady = static_cast<sem_t*>(m_dataReadySem);
//     auto* slotFree = static_cast<sem_t*>(m_slotFreeSem);

//     auto status = waitSemaphore(dataReady, timeout);

//     if (!status) {
//         return Result<Message>::failure(status.error());
//     }

//     status = waitSemaphore(mutex, timeout);

//     if (!status) {
//         postSemaphore(dataReady);
//         return Result<Message>::failure(status.error());
//     }

//     auto* header = static_cast<ShmemHeader*>(m_memory);

//     if (header->magic != ShmemMagic ||
//         header->size == 0 ||
//         header->size > header->capacity) {
//         postSemaphore(mutex);
//         postSemaphore(slotFree);
//         return Result<Message>::failure(CommError::InvalidMessage);
//     }

//     const auto* payloadMemory =
//         static_cast<const std::byte*>(m_memory) + sizeof(ShmemHeader);

//     std::vector<std::byte> encodedMessage(
//         payloadMemory,
//         payloadMemory + header->size
//     );

//     header->size = 0;

//     status = postSemaphore(mutex);

//     if (!status) {
//         return Result<Message>::failure(status.error());
//     }

//     status = postSemaphore(slotFree);

//     if (!status) {
//         return Result<Message>::failure(status.error());
//     }

//     return decodeMessage(encodedMessage);
// }

Result<Message> ShmemTransport::receive(
    std::chrono::milliseconds timeout
)
{
    if (!isOpen()) {
        return Result<Message>::failure(CommError::NotOpen);
    }

    auto* mutex = static_cast<sem_t*>(m_mutexSem);

    auto status = waitSemaphore(mutex, timeout);

    if (!status) {
        return Result<Message>::failure(status.error());
    }

    auto* header = static_cast<ShmemHeader*>(m_memory);

    if (header->magic != ShmemMagic ||
        header->size == 0 ||
        header->size > header->capacity) {
        postSemaphore(mutex);
        return Result<Message>::failure(CommError::Timeout);
    }

    const auto* payloadMemory =
        static_cast<const std::byte*>(m_memory) + sizeof(ShmemHeader);

    std::vector<std::byte> encodedMessage(
        payloadMemory,
        payloadMemory + header->size
    );

    status = postSemaphore(mutex);

    if (!status) {
        return Result<Message>::failure(status.error());
    }

    return decodeMessage(encodedMessage);
}

} // namespace odin_comm
} // namespace odin