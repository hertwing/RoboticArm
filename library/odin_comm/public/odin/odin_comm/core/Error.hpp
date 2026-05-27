#ifndef ERROR_HPP
#define ERROR_HPP

#include <string_view>

namespace odin
{
namespace odin_comm
{

enum class CommError
{
    None = 0,

    NotOpen,
    AlreadyOpen,
    Timeout,

    InvalidArgument,
    InvalidMessage,
    InvalidMagic,
    UnsupportedVersion,

    PayloadTooLarge,
    PayloadSizeMismatch,
    ChecksumMismatch,

    SerializationFailed,
    DeserializationFailed,

    TransportError,
    SendFailed,
    ReceiveFailed,

    AckTimeout,
    DuplicateMessage,

    EnvironmentVariableMissing,
    UnknownNode,
    NodeNotFound,

    EndpointNotFound,
    AmbiguousEndpoint,
    InvalidEndpointRole,
    InvalidEndpointConfig,
    PeerNodeNotFound,
};

[[nodiscard]] constexpr std::string_view toString(CommError error) noexcept
{
    switch (error) {
        case CommError::None:
            return "None";
        case CommError::NotOpen:
            return "NotOpen";
        case CommError::AlreadyOpen:
            return "AlreadyOpen";
        case CommError::Timeout:
            return "Timeout";
        case CommError::InvalidArgument:
            return "InvalidArgument";
        case CommError::InvalidMessage:
            return "InvalidMessage";
        case CommError::InvalidMagic:
            return "InvalidMagic";
        case CommError::UnsupportedVersion:
            return "UnsupportedVersion";
        case CommError::PayloadTooLarge:
            return "PayloadTooLarge";
        case CommError::PayloadSizeMismatch:
            return "PayloadSizeMismatch";
        case CommError::ChecksumMismatch:
            return "ChecksumMismatch";
        case CommError::SerializationFailed:
            return "SerializationFailed";
        case CommError::DeserializationFailed:
            return "DeserializationFailed";
        case CommError::TransportError:
            return "TransportError";
        case CommError::SendFailed:
            return "SendFailed";
        case CommError::ReceiveFailed:
            return "ReceiveFailed";
        case CommError::AckTimeout:
            return "AckTimeout";
        case CommError::DuplicateMessage:
            return "DuplicateMessage";
        case CommError::EnvironmentVariableMissing:
            return "EnvironmentVariableMissing";
        case CommError::UnknownNode:
            return "UnknownNode";
        case CommError::NodeNotFound:
            return "NodeNotFound";
        case CommError::EndpointNotFound:
            return "EndpointNotFound";
        case CommError::AmbiguousEndpoint:
            return "AmbiguousEndpoint";
        case CommError::InvalidEndpointRole:
            return "InvalidEndpointRole";
        case CommError::InvalidEndpointConfig:
            return "InvalidEndpointConfig";
        case CommError::PeerNodeNotFound:
            return "PeerNodeNotFound";
    }

    return "Unknown";
}

} // namespace odin_comm
} // namespace odin

#endif // ERROR_HPP