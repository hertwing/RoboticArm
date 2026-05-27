#ifndef SUBSCRIBER_HPP
#define SUBSCRIBER_HPP

#include <chrono>
#include <utility>

#include "odin/odin_comm/core/Error.hpp"
#include "odin/odin_comm/core/MessageType.hpp"
#include "odin/odin_comm/core/Result.hpp"
#include "odin/odin_comm/core/Serializer.hpp"
#include "odin/odin_comm/messaging/MessageTraits.hpp"
#include "odin/odin_comm/messaging/Messenger.hpp"

namespace odin
{
namespace odin_comm
{

template <TriviallySerializable T>
class Subscriber
{
public:
    explicit Subscriber(Messenger& messenger)
        requires HasMessageTraits<T>
        : m_messenger{messenger}
        , m_expectedMessageType{MessageTraits<T>::messageType}
    {
    }

    Subscriber(
        Messenger& messenger,
        MessageType expectedMessageType
    )
        : m_messenger{messenger}
        , m_expectedMessageType{expectedMessageType}
    {
    }

    [[nodiscard]] Result<T> receive(
        std::chrono::milliseconds timeout
    )
    {
        auto messageResult = m_messenger.receive(timeout);

        if (!messageResult) {
            return Result<T>::failure(messageResult.error());
        }

        const auto& message = messageResult.value();

        if (message.header.type != m_expectedMessageType) {
            return Result<T>::failure(CommError::InvalidMessage);
        }

        auto decoded = deserializeTrivial<T>(message.payload);

        if (!decoded) {
            return Result<T>::failure(CommError::DeserializationFailed);
        }

        return Result<T>::success(std::move(*decoded));
    }

private:
    Messenger& m_messenger;
    MessageType m_expectedMessageType{MessageType::Unknown};
};

} // namespace odin_comm
} // namespace odin

#endif // SUBSCRIBER_HPP