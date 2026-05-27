#ifndef ODIN_COMM_TRANSPORT_TRAITS_HPP
#define ODIN_COMM_TRANSPORT_TRAITS_HPP

#include <concepts>

#include "odin/odin_comm/config/TransportKind.hpp"

namespace odin
{
namespace odin_comm
{

struct Udp
{
};

struct Shmem
{
};

template <typename Transport>
struct TransportTraits;

template <>
struct TransportTraits<Udp>
{
    static constexpr TransportKind transportKind = TransportKind::Udp;
};

template <>
struct TransportTraits<Shmem>
{
    static constexpr TransportKind transportKind = TransportKind::Shmem;
};

template <typename Transport>
concept CommunicationTransport = requires
{
    { TransportTraits<Transport>::transportKind } -> std::convertible_to<TransportKind>;
};

} // namespace odin_comm
} // namespace odin

#endif // ODIN_COMM_TRANSPORT_TRAITS_HPP