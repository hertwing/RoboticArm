#ifndef MESSENGER_HPP
#define MESSENGER_HPP

#include <algorithm>
#include <chrono>
#include <deque>
#include <utility>

#include "odin/odin_comm/core/Message.hpp"
#include "odin/odin_comm/core/MessageFactory.hpp"
#include "odin/odin_comm/core/MessageFlags.hpp"
#include "odin/odin_comm/core/MessageValidation.hpp"
#include "odin/odin_comm/core/Result.hpp"
#include "odin/odin_comm/core/SequenceGenerator.hpp"
#include "odin/odin_comm/core/Status.hpp"
#include "odin/odin_comm/messaging/RetryPolicy.hpp"
#include "odin/odin_comm/transport/ITransport.hpp"

namespace odin
{
namespace odin_comm
{

class Messenger
{
public:
    explicit Messenger(ITransport& transport)
        : m_transport{transport}
    {
    }

    [[nodiscard]] Status send(
        Message message,
        std::chrono::milliseconds timeout
    )
    {
        prepareOutgoingMessage(message);

        const auto validationStatus = validateMessage(message);

        if (!validationStatus) {
            return validationStatus;
        }

        return m_transport.send(message, timeout);
    }

    [[nodiscard]] Status sendReliable(
        Message message,
        std::chrono::milliseconds timeout
    )
    {
        return sendReliable(std::move(message), timeout, RetryPolicy{});
    }

    [[nodiscard]] Status sendReliable(
        Message message,
        std::chrono::milliseconds timeout,
        const RetryPolicy& retryPolicy
    )
    {
        if (retryPolicy.maxAttempts == 0) {
            return Status::failure(CommError::InvalidArgument);
        }

        prepareOutgoingMessage(message);

        message.header.flags |= MessageFlags::AckRequired;

        const auto sentSequenceId = message.header.sequence_id;

        const auto validationStatus = validateMessage(message);

        if (!validationStatus) {
            return validationStatus;
        }

        const auto operationDeadline = std::chrono::steady_clock::now() + timeout;

        for (std::uint32_t attempt = 0; attempt < retryPolicy.maxAttempts; ++attempt) {
            const auto now = std::chrono::steady_clock::now();

            if (now >= operationDeadline) {
                return Status::failure(CommError::AckTimeout);
            }

            const auto remainingOperationTime =
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    operationDeadline - now
                );

            const auto sendStatus = m_transport.send(message, remainingOperationTime);

            if (!sendStatus) {
                return sendStatus;
            }

            const auto waitTimeout = std::min(
                retryPolicy.ackTimeout,
                remainingOperationTime
            );

            const auto ackStatus = waitForAck(
                sentSequenceId,
                waitTimeout
            );

            if (ackStatus) {
                return Status::ok();
            }

            if (ackStatus.error() != CommError::AckTimeout &&
                ackStatus.error() != CommError::Timeout) {
                return ackStatus;
            }
        }

        return Status::failure(CommError::AckTimeout);
    }

    [[nodiscard]] Result<Message> receive(
    std::chrono::milliseconds timeout)
    {
        if (!m_inbox.empty()) {
            auto message = std::move(m_inbox.front());
            m_inbox.pop_front();

            return Result<Message>::success(std::move(message));
        }

        const auto deadline = std::chrono::steady_clock::now() + timeout;

        while (std::chrono::steady_clock::now() < deadline) {
            const auto now = std::chrono::steady_clock::now();
            const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
                deadline - now
            );

            auto result = m_transport.receive(remaining);

            if (!result) {
                return result;
            }

            auto processed = processIncomingApplicationMessage(
                std::move(result.value()),
                remaining
            );

            if (processed) {
                return processed;
            }

            if (processed.error() == CommError::DuplicateMessage ||
                processed.error() == CommError::InvalidMessage) {
                continue;
            }

            return processed;
        }

        return Result<Message>::failure(CommError::Timeout);
    }

private:
    void prepareOutgoingMessage(Message& message)
    {
        if (message.header.sequence_id == 0) {
            message.header.sequence_id = m_sequenceGenerator.next();
        }

        if (message.header.correlation_id == 0) {
            message.header.correlation_id = message.header.sequence_id;
        }
    }

    [[nodiscard]] Status waitForAck(
        SequenceId expectedSequenceId,
        std::chrono::milliseconds timeout
    )
    {
        const auto deadline = std::chrono::steady_clock::now() + timeout;

        while (std::chrono::steady_clock::now() < deadline) {
            const auto now = std::chrono::steady_clock::now();
            const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
                deadline - now
            );

            auto result = m_transport.receive(remaining);

            if (!result) {
                if (result.error() == CommError::Timeout) {
                    return Status::failure(CommError::AckTimeout);
                }

                return Status::failure(result.error());
            }

            auto receivedMessage = std::move(result.value());

            const auto receivedValidationStatus = validateMessage(receivedMessage);

            if (!receivedValidationStatus) {
                return receivedValidationStatus;
            }

            if (isAck(receivedMessage.header.flags)) {
                if (receivedMessage.header.correlation_id == expectedSequenceId) {
                    return Status::ok();
                }

                continue;
            }

            auto processed = processIncomingApplicationMessage(
                std::move(receivedMessage),
                remaining
            );

            if (processed) {
                m_inbox.push_back(std::move(processed.value()));
                continue;
            }

            if (processed.error() == CommError::DuplicateMessage ||
                processed.error() == CommError::InvalidMessage) {
                continue;
            }

            return Status::failure(processed.error());
        }

        return Status::failure(CommError::AckTimeout);
    }

    [[nodiscard]] Status sendAckFor(
        const Message& message,
        std::chrono::milliseconds timeout
    )
    {
        const auto ack = createAckMessage(
            m_sequenceGenerator.next(),
            message.header.sequence_id
        );

        const auto validationStatus = validateMessage(ack);

        if (!validationStatus) {
            return validationStatus;
        }

        return m_transport.send(ack, timeout);
    }

    [[nodiscard]] bool wasRecentlyReceived(SequenceId sequenceId) const
    {
        return std::find(
            m_recentReceivedSequences.begin(),
            m_recentReceivedSequences.end(),
            sequenceId
        ) != m_recentReceivedSequences.end();
    }

    void rememberReceived(SequenceId sequenceId)
    {
        if (sequenceId == 0) {
            return;
        }

        if (wasRecentlyReceived(sequenceId)) {
            return;
        }

        m_recentReceivedSequences.push_back(sequenceId);

        while (m_recentReceivedSequences.size() > m_maxRecentReceivedSequences) {
            m_recentReceivedSequences.pop_front();
        }
    }

    [[nodiscard]] Result<Message> processIncomingApplicationMessage(
    Message message,
    std::chrono::milliseconds timeout)
    {
        const auto validationStatus = validateMessage(message);

        if (!validationStatus) {
            return Result<Message>::failure(validationStatus.error());
        }

        if (isAck(message.header.flags)) {
            return Result<Message>::failure(CommError::InvalidMessage);
        }

        if (!requiresAck(message.header.flags)) {
            return Result<Message>::success(std::move(message));
        }

        const auto isDuplicate = wasRecentlyReceived(message.header.sequence_id);

        const auto ackStatus = sendAckFor(message, timeout);

        if (!ackStatus) {
            return Result<Message>::failure(ackStatus.error());
        }

        if (isDuplicate) {
            return Result<Message>::failure(CommError::DuplicateMessage);
        }

        rememberReceived(message.header.sequence_id);

        return Result<Message>::success(std::move(message));
    }

    ITransport& m_transport;
    SequenceGenerator m_sequenceGenerator{};
    std::deque<Message> m_inbox{};

    std::deque<SequenceId> m_recentReceivedSequences{};
    std::size_t m_maxRecentReceivedSequences{128};
};

} // namespace odin_comm
} // namespace odin

#endif // MESSENGER_HPP