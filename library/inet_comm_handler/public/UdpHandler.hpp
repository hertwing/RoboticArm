#ifndef UDPHANDLER_H
#define UDPHANDLER_H

#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <cerrno>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>
#include <thread>

template <typename T>
class UdpHandler
{
public:
    // Receiver (bind on port, INADDR_ANY)
    UdpHandler(std::uint64_t buffer_size, const std::uint16_t& port);
    // Sender (connect to receiver_ip:port)
    UdpHandler(std::uint64_t buffer_size, const std::uint16_t& port, const std::string& receiver_ip);

    ~UdpHandler();

    // (Re)create UDP socket (idempotent); returns 0 on success
    std::int8_t createUdpSocket();

    // Fixed-size API (uses m_buffer_size == sizeof(T) or user-chosen):
    // returns:  1 = got packet, 0 = timeout, -1 = error
    std::int8_t read(T* buffer, int timeout_us = 100000);
    // returns true on success (entire datagram sent), false on error/timeout
    bool write(const T* buffer, int timeout_us = 100000);

    // Variable-size API (for datagrams != m_buffer_size):
    // out_len = actual datagram size received (<= max_len)
    std::int8_t read(T* buffer, size_t max_len, size_t& out_len, int timeout_us = 100000);
    bool write(const T* buffer, size_t len, int timeout_us = 100000);

    // Optional tuning
    void set_send_buffer_bytes(int bytes);
    void set_recv_buffer_bytes(int bytes);
    void set_ttl(int hops);
    void set_dscp(uint8_t dscp);      // 0..63 - DS field (shifted into TOS)
    void enable_broadcast(bool on);
    int native_fd() const;
private:
    enum class Role { Receiver, Sender };
    int  select_with_deadline(int fd, bool want_read, bool want_write, int timeout_us) const;

    std::uint64_t m_buffer_size;
    int           m_sockfd;
    std::uint16_t m_port;
    std::string   m_receiver_ip;
    Role          m_role;
    sockaddr_in   m_peer_addr;  // for Sender: connected peer; for Receiver: last source (updated by recvfrom)
};

template <typename T>
UdpHandler<T>::UdpHandler(std::uint64_t buffer_size, const std::uint16_t& port)
    : m_buffer_size(buffer_size),
      m_sockfd(-1),
      m_port(port),
      m_receiver_ip(),
      m_role(Role::Receiver),
      m_peer_addr{}
{
    while (createUdpSocket() != 0) {
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }
}

template <typename T>
UdpHandler<T>::UdpHandler(std::uint64_t buffer_size, const std::uint16_t& port, const std::string& receiver_ip)
    : m_buffer_size(buffer_size),
      m_sockfd(-1),
      m_port(port),
      m_receiver_ip(receiver_ip),
      m_role(Role::Sender),
      m_peer_addr{}
{
    while (createUdpSocket() != 0)
    {
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }
}

template <typename T>
UdpHandler<T>::~UdpHandler()
{
    if (m_sockfd >= 0) close(m_sockfd);
    m_sockfd = -1;
}

template <typename T>
std::int8_t UdpHandler<T>::createUdpSocket()
{
    if (m_sockfd >= 0)
    {
        close(m_sockfd);
        m_sockfd = -1;
    }

    m_sockfd = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (m_sockfd < 0)
    {
        std::cerr << "[UDP] socket() failed: " << strerror(errno) << std::endl;
        return -1;
    }

    int reuse = 1;
    if (::setsockopt(m_sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0)
    {
        std::cerr << "[UDP] setsockopt(SO_REUSEADDR) failed: " << strerror(errno) << std::endl;
    }
#ifdef SO_REUSEPORT
    if (::setsockopt(m_sockfd, SOL_SOCKET, SO_REUSEPORT, &reuse, sizeof(reuse)) < 0)
    {
        if (errno != ENOPROTOOPT)
            std::cerr << "[UDP] setsockopt(SO_REUSEPORT) failed: " << strerror(errno) << std::endl;
    }
#endif
    // TODO: Move to Data header
    int snd = 4 * 1024 * 1024;
    int rcv = 4 * 1024 * 1024;
    ::setsockopt(m_sockfd, SOL_SOCKET, SO_SNDBUF, &snd, sizeof(snd));
    ::setsockopt(m_sockfd, SOL_SOCKET, SO_RCVBUF, &rcv, sizeof(rcv));

    std::memset(&m_peer_addr, 0, sizeof(m_peer_addr));
    m_peer_addr.sin_family = AF_INET;
    m_peer_addr.sin_port   = htons(m_port);

    if (m_role == Role::Receiver)
    {
        m_peer_addr.sin_addr.s_addr = htonl(INADDR_ANY);
        if (::bind(m_sockfd, reinterpret_cast<sockaddr*>(&m_peer_addr), sizeof(m_peer_addr)) < 0)
        {
            std::cerr << "[UDP] bind(" << m_port << ") failed: " << strerror(errno) << std::endl;
            close(m_sockfd); m_sockfd = -1;
            return -1;
        }
        std::cout << "[UDP] Receiver bound on *:" << m_port << std::endl;
    } 
    else 
    {
        if (::inet_pton(AF_INET, m_receiver_ip.c_str(), &m_peer_addr.sin_addr) != 1) {
            std::cerr << "[UDP] inet_pton(" << m_receiver_ip << ") failed: " << strerror(errno) << std::endl;
            close(m_sockfd); m_sockfd = -1;
            return -1;
        }
        if (::connect(m_sockfd, reinterpret_cast<sockaddr*>(&m_peer_addr), sizeof(m_peer_addr)) < 0) {
            std::cerr << "[UDP] connect(" << m_receiver_ip << ":" << m_port << ") failed: "
                      << strerror(errno) << std::endl;
            close(m_sockfd); m_sockfd = -1;
            return -1;
        }
        std::cout << "[UDP] Sender connected to " << m_receiver_ip << ":" << m_port << std::endl;
    }

    return 0;
}

template <typename T>
int UdpHandler<T>::select_with_deadline(int fd, bool r, bool w, int timeout_us) const
{
    fd_set rset, wset; fd_set* R=nullptr; fd_set* W=nullptr;
    if (r) { FD_ZERO(&rset); FD_SET(fd, &rset); R=&rset; }
    if (w) { FD_ZERO(&wset); FD_SET(fd, &wset); W=&wset; }

    timeval tv{0,0};
    if (timeout_us > 0) {
        tv.tv_sec  = timeout_us / 1000000;
        tv.tv_usec = timeout_us % 1000000;
    }
    return ::select(fd+1, R, W, nullptr, &tv); // 0 - „got nothing right now”
}

// ===== Fixed-size API =====

template <typename T>
std::int8_t UdpHandler<T>::read(T* buffer, int timeout_us)
{
    size_t out_len = 0;
    std::int8_t rc = read(buffer, m_buffer_size, out_len, timeout_us);
    if (rc == 1 && out_len != m_buffer_size) {
        std::cerr << "[UDP] read: datagram size " << out_len << " != expected " << m_buffer_size << std::endl;
    }
    return rc;
}

template <typename T>
bool UdpHandler<T>::write(const T* buffer, int timeout_us)
{
    return write(buffer, m_buffer_size, timeout_us);
}

// ===== Variable-size API =====

template <typename T>
std::int8_t UdpHandler<T>::read(T* buffer, size_t max_len, size_t& out_len, int timeout_us)
{
    if (m_sockfd < 0)
    {
        std::cerr << "[UDP] read: invalid socket" << std::endl;
        errno = ENOTCONN;
        return -1;
    }

    int rc = select_with_deadline(m_sockfd, /*read*/true, /*write*/false, timeout_us);
    if (rc <= 0)
    {
        if (rc == 0) return 0; // timeout
        std::cerr << "[UDP] select(read) failed: " << strerror(errno) << std::endl;
        return -1;
    }

    sockaddr_in src{}; socklen_t slen = sizeof(src);
    ssize_t n = ::recvfrom(m_sockfd, reinterpret_cast<void*>(buffer),
                           max_len, 0, reinterpret_cast<sockaddr*>(&src), &slen);
    if (n < 0)
    {
        if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) return 0;
        std::cerr << "[UDP] recvfrom failed: " << strerror(errno) << std::endl;
        return -1;
    }

    if (m_role == Role::Receiver)
    {
        m_peer_addr = src;
    }

    out_len = static_cast<size_t>(n);
    return 1;
}

template <typename T>
bool UdpHandler<T>::write(const T* buffer, size_t len, int timeout_us)
{
    if (m_sockfd < 0)
    {
        std::cerr << "[UDP] write: invalid socket" << std::endl;
        errno = ENOTCONN;
        return false;
    }

    int rc = select_with_deadline(m_sockfd, /*read*/false, /*write*/true, timeout_us);
    if (rc <= 0)
    {
        if (rc == 0) errno = EAGAIN; // timeout
        return false;
    }

    ssize_t n;
    if (m_role == Role::Sender)
    {
        n = ::send(m_sockfd, reinterpret_cast<const void*>(buffer), len, 0);
    } 
    else 
    {
        n = ::sendto(m_sockfd, reinterpret_cast<const void*>(buffer), len, 0,
                     reinterpret_cast<const sockaddr*>(&m_peer_addr), sizeof(m_peer_addr));
    }

    if (n < 0)
    {
        if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) return false;
        std::cerr << "[UDP] send/sendto failed: " << strerror(errno) << std::endl;
        return false;
    }
    return static_cast<size_t>(n) == len;
}

// ===== Tuning =====

template <typename T>
void UdpHandler<T>::set_send_buffer_bytes(int bytes)
{
    if (m_sockfd >= 0) ::setsockopt(m_sockfd, SOL_SOCKET, SO_SNDBUF, &bytes, sizeof(bytes));
}

template <typename T>
void UdpHandler<T>::set_recv_buffer_bytes(int bytes)
{
    if (m_sockfd >= 0) ::setsockopt(m_sockfd, SOL_SOCKET, SO_RCVBUF, &bytes, sizeof(bytes));
}

template <typename T>
void UdpHandler<T>::set_ttl(int hops)
{
    if (m_sockfd >= 0) ::setsockopt(m_sockfd, IPPROTO_IP, IP_TTL, &hops, sizeof(hops));
}

template <typename T>
void UdpHandler<T>::set_dscp(uint8_t dscp)
{
    // DSCP (6 bitów) trafia do TOS << 2 (ECN)
    uint8_t tos = static_cast<uint8_t>((dscp & 0x3F) << 2);
    if (m_sockfd >= 0) ::setsockopt(m_sockfd, IPPROTO_IP, IP_TOS, &tos, sizeof(tos));
}

template <typename T>
void UdpHandler<T>::enable_broadcast(bool on)
{
    int v = on ? 1 : 0;
    if (m_sockfd >= 0) ::setsockopt(m_sockfd, SOL_SOCKET, SO_BROADCAST, &v, sizeof(v));
}

template <typename T>
int UdpHandler<T>::native_fd() const
{
    return m_sockfd;
}

#endif // UDPHANDLER_H
