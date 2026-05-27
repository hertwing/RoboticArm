#ifndef PUBLISHER_HPP
#define PUBLISHER_HPP

#include <chrono>
#include <utility>

#include "odin/odin_comm/core/MessageFactory.hpp"
#include "odin/odin_comm/core/MessageFlags.hpp"
#include "odin/odin_comm/core/MessageType.hpp"
#include "odin/odin_comm/core/Serializer.hpp"
#include "odin/odin_comm/core/Status.hpp"
#include "odin/odin_comm/messaging/MessageTraits.hpp"
#include "odin/odin_comm/messaging/Messenger.hpp"
#include "odin/odin_comm/messaging/RetryPolicy.hpp"

namespace odin
{
namespace odin_comm
{

template <TriviallySerializable T>
class Publisher
{
public:
    explicit Publisher(Messenger& messenger)
        requires HasMessageTraits<T>
        : m_messenger{messenger}
        , m_messageType{MessageTraits<T>::messageType}
    {
    }

    Publisher(
        Messenger& messenger,
        MessageType messageType
    )
        : m_messenger{messenger}
        , m_messageType{messageType}
    {
    }

    [[nodiscard]] Status publish(
        const T& value,
        std::chrono::milliseconds timeout
    )
    {
        return publishWithFlags(
            value,
            MessageFlags::None,
            timeout
        );
    }

    [[nodiscard]] Status publishReliable(
        const T& value,
        std::chrono::milliseconds timeout
    )
    {
        return publishReliable(
            value,
            timeout,
            RetryPolicy{}
        );
    }

    [[nodiscard]] Status publishReliable(
        const T& value,
        std::chrono::milliseconds timeout,
        const RetryPolicy& retryPolicy
    )
    {
        const auto payload = serializeTrivial(value);

        auto messageResult = createMessage(
            m_messageType,
            MessageFlags::None,
            0,
            0,
            payload
        );

        if (!messageResult) {
            return Status::failure(messageResult.error());
        }

        return m_messenger.sendReliable(
            std::move(messageResult.value()),
            timeout,
            retryPolicy
        );
    }

    [[nodiscard]] Status publishWithFlags(
        const T& value,
        MessageFlags flags,
        std::chrono::milliseconds timeout
    )
    {
        const auto payload = serializeTrivial(value);

        auto messageResult = createMessage(
            m_messageType,
            flags,
            0,
            0,
            payload
        );

        if (!messageResult) {
            return Status::failure(messageResult.error());
        }

        return m_messenger.send(
            std::move(messageResult.value()),
            timeout
        );
    }

private:
    Messenger& m_messenger;
    MessageType m_messageType{MessageType::Unknown};
};

} // namespace odin_comm
} // namespace odin

#endif // PUBLISHER_HPP