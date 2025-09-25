#ifndef UDPHANDLER_H
#define UDPHANDLER_H

#include "InetCommData.h"

#include <arpa/inet.h>
#include <sys/uio.h>
#include <utility>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <time.h> 

#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>
#include <thread>
#include <stdexcept>

template <typename T>
class UdpHandler
{
public:
    // Receiver (bind on port, INADDR_ANY)
    UdpHandler(std::uint64_t buffer_size, const std::uint16_t& port);
    // Sender (connect to receiver_ip:port)
    UdpHandler(std::uint64_t buffer_size, const std::uint16_t& port, const std::string& receiver_ip);

    UdpHandler(const UdpHandler&) = delete;
    UdpHandler & operator=(const UdpHandler&) = delete;

    UdpHandler(UdpHandler && other) noexcept :
        m_buffer_size(other.m_buffer_size),
        m_sockfd(std::exchange(other.m_sockfd, -1)),
        m_port(other.m_port),
        m_receiver_ip(std::move(other.m_receiver_ip)),
        m_role(other.m_role),
        m_peer_addr(other.m_peer_addr) {}

    UdpHandler & operator=(UdpHandler && other) noexcept
    {
        if (this != &other)
        {
            if (m_sockfd >= 0) ::close(m_sockfd);
            m_buffer_size = other.m_buffer_size;
            m_sockfd      = std::exchange(other.m_sockfd, -1);
            m_port        = other.m_port;
            m_receiver_ip = std::move(other.m_receiver_ip);
            m_role        = other.m_role;
            m_peer_addr   = other.m_peer_addr;
        }
        return *this;
    }

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

    // Drain loop for high-PPS streams (video): read as many pending datagrams as possible
    // within limits; returns 1 if at least one packet was read (buffer holds the LAST one),
    // 0 on timeout/no data, -1 on error.
    std::int8_t drain_latest(T* buffer, size_t max_len, size_t& out_len,
                             int timeout_us = 100000, int max_packets = 64, int max_us = 2000);

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
    bool          m_has_peer;   // Receiver: set after first successful read()
};

template <typename T>
UdpHandler<T>::UdpHandler(std::uint64_t buffer_size, const std::uint16_t& port) : 
    m_buffer_size(buffer_size),
    m_sockfd(-1),
    m_port(port),
    m_receiver_ip(),
    m_role(Role::Receiver),
    m_peer_addr{},
    m_has_peer(false)
{
    int tries = 0, max_tries = 10;
    while (createUdpSocket() != 0 && ++tries < max_tries)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(250 * tries));
    }
    if (m_sockfd < 0) throw std::runtime_error("UDP init failed");
}

template <typename T>
UdpHandler<T>::UdpHandler(std::uint64_t buffer_size, const std::uint16_t& port, const std::string& receiver_ip) :
    m_buffer_size(buffer_size),
    m_sockfd(-1),
    m_port(port),
    m_receiver_ip(receiver_ip),
    m_role(Role::Sender),
    m_peer_addr{},
    m_has_peer(false)
{
    int tries = 0, max_tries = 10;
    while (createUdpSocket() != 0 && ++tries < max_tries)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(250 * tries)); // backoff
    }
    if (m_sockfd < 0) throw std::runtime_error("UDP init failed");
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

    // Set nonblocking mode, control I/O with select_with_deadline
    int flags = ::fcntl(m_sockfd, F_GETFL, 0);
    if (flags >= 0)
    {
        if (::fcntl(m_sockfd, F_SETFL, flags | O_NONBLOCK) < 0) {
            std::cerr << "[UDP] fcntl(O_NONBLOCK) failed: " << strerror(errno) << std::endl;
        }
    } else {
        std::cerr << "[UDP] fcntl(F_GETFL) failed: " << strerror(errno) << std::endl;
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
    int snd = UDP_BUFF;
    int rcv = UDP_BUFF;
    if (::setsockopt(m_sockfd, SOL_SOCKET, SO_SNDBUF, &snd, sizeof(snd)) < 0)
    {
        std::cerr << "[UDP] setsockopt(SO_SNDBUF) failed: " << strerror(errno) << std::endl;
    }
    if (::setsockopt(m_sockfd, SOL_SOCKET, SO_RCVBUF, &rcv, sizeof(rcv)) < 0)
    {
        std::cerr << "[UDP] setsockopt(SO_RCVBUF) failed: " << strerror(errno) << std::endl;
    }
    // Log effective data set by kernel
    socklen_t optlen = sizeof(int);
    int eff_snd = 0, eff_rcv = 0;
    if (::getsockopt(m_sockfd, SOL_SOCKET, SO_SNDBUF, &eff_snd, &optlen) == 0 &&
        ::getsockopt(m_sockfd, SOL_SOCKET, SO_RCVBUF, &eff_rcv, &optlen) == 0)
    {
        std::cout << "[UDP] Buffers: snd=" << eff_snd << "B, rcv=" << eff_rcv << "B\n";
    }

    std::memset(&m_peer_addr, 0, sizeof(m_peer_addr));
    m_peer_addr.sin_family = AF_INET;
    m_peer_addr.sin_port   = htons(m_port);

    m_has_peer = false;

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
        if (::inet_pton(AF_INET, m_receiver_ip.c_str(), &m_peer_addr.sin_addr) != 1)
        {
            std::cerr << "[UDP] inet_pton(" << m_receiver_ip << ") failed: " << strerror(errno) << std::endl;
            close(m_sockfd); m_sockfd = -1;
            return -1;
        }
        if (::connect(m_sockfd, reinterpret_cast<sockaddr*>(&m_peer_addr), sizeof(m_peer_addr)) < 0)
        {
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
int UdpHandler<T>::select_with_deadline(int fd, bool want_read, bool want_write, int timeout_us) const
{
    // poll (timeout == 0) -> exactly one select()
    if (timeout_us == 0)
    {
        while (true)
        {
            fd_set rset, wset; fd_set *R=nullptr, *W=nullptr;
            if (want_read) 
            {
                FD_ZERO(&rset);
                FD_SET(fd, &rset);
                R=&rset;
            }
            if (want_write)
            {
                FD_ZERO(&wset);
                FD_SET(fd, &wset);
                W=&wset;
            }
            timeval tv{0,0};
            int rc = ::select(fd+1, R, W, nullptr, &tv);
            if (rc >= 0) return rc;           // 0 = nothing to read/write, >0 = done
            if (errno == EINTR) continue;     // repeat poll after signal
            return rc;                         // another error
        }
    }

    // timeout > 0: wait til deadline
    if (timeout_us > 0) 
    {
        const auto start = std::chrono::steady_clock::now();
        while (true) 
        {
            auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
                               std::chrono::steady_clock::now() - start).count();
            if (elapsed >= timeout_us) return 0; // timeout
            int left = timeout_us - static_cast<int>(elapsed);
            timeval tv{ left / 1000000, left % 1000000 };
            fd_set rset, wset; fd_set *R=nullptr, *W=nullptr;
            if (want_read) 
            {
                FD_ZERO(&rset);
                FD_SET(fd, &rset);
                R=&rset;
            }
            if (want_write)
            {
                FD_ZERO(&wset);
                FD_SET(fd, &wset);
                W=&wset;
            }
            int rc = ::select(fd+1, R, W, nullptr, &tv);
            if (rc >= 0) return rc;
            if (errno == EINTR) continue;
            return rc;
        }
    }

    // timeout < 0: infinit wait (with retry on EINTR)
    while (true)
    {
        fd_set rset, wset; fd_set *R=nullptr, *W=nullptr;
        if (want_read) 
        {
            FD_ZERO(&rset);
            FD_SET(fd, &rset);
            R=&rset;
        }
        if (want_write)
        {
            FD_ZERO(&wset);
            FD_SET(fd, &wset);
            W=&wset;
        }
        int rc = ::select(fd+1, R, W, nullptr, nullptr);
        if (rc >= 0) return rc;
        if (errno == EINTR) continue;
        return rc;
    }
}

// ===== Fixed-size API =====

template <typename T>
std::int8_t UdpHandler<T>::read(T* buffer, int timeout_us)
{
    size_t out_len = 0;
    std::int8_t rc = read(buffer, m_buffer_size, out_len, timeout_us);
    if (rc == 1 && out_len != m_buffer_size)
    {
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

    // detect cut datagrams
    sockaddr_in src{}; socklen_t slen = sizeof(src);
    iovec iov{};
    iov.iov_base = static_cast<void*>(buffer);
    iov.iov_len  = max_len;
    msghdr msg{};
    msg.msg_name    = &src;
    msg.msg_namelen = slen;
    msg.msg_iov     = &iov;
    msg.msg_iovlen  = 1;

    ssize_t n = ::recvmsg(m_sockfd, &msg, MSG_TRUNC);
    if (n < 0)
    {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
        {
            // No ready data despite previous select() - acceptable with O_NONBLOCK
            return 0; // timeout-like
        }
        if (errno == EINTR)
        {
            std::cerr << "[UDP] recvmsg interrupted (EINTR)\n";
            return -1; // interrupt, not timeout
        }
        std::cerr << "[UDP] recvmsg failed: " << strerror(errno) << std::endl;
        return -1;
    }
    if (static_cast<size_t>(n) > max_len || (msg.msg_flags & MSG_TRUNC))
    {
        std::cerr << "[UDP] recv: datagram truncated (size=" << n
                  << " > buf=" << max_len << "), dropping\n";
        errno = EMSGSIZE;
        return -1;
    }
    if (m_role == Role::Receiver)
    {
        m_peer_addr = src;
        m_has_peer  = true;
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
    // UDP limit
    if (len > 65507u)
    {
        std::cerr << "[UDP] write: payload too large (" << len << " > 65507), refusing\n";
        errno = EMSGSIZE;
        return false;
    }
    // MTU guard: alarm typical Ethernet MTU ~1500 (payload ~1472)
    if (len >= 1472u)
    {
        static thread_local uint64_t warn_cnt = 0;
        if (warn_cnt < 5)
        { // rate-limit
            std::cerr << "[UDP] write: payload " << len
                      << "B may fragment on common MTU (≥1472). Consider chunking.\n";
            ++warn_cnt;
        }
    }
    // No known peer on receiver
    if (m_role == Role::Receiver && !m_has_peer)
    {
        errno = EDESTADDRREQ;
        std::cerr << "[UDP] write: no peer learned yet (Receiver). Drop.\n";
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
        if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) return false;
        std::cerr << "[UDP] send/sendto failed: " << strerror(errno) << std::endl;
        return false;
    }
    return static_cast<size_t>(n) == len;
}

template <typename T>
std::int8_t UdpHandler<T>::drain_latest(T* buffer, size_t max_len, size_t& out_len,
                                        int timeout_us, int max_packets, int max_us)
{
    out_len = 0;
    if (m_sockfd < 0) {
        std::cerr << "[UDP] drain_latest: invalid socket\n";
        errno = ENOTCONN;
        return -1;
    }
    // Wait for first packet (deadline)
    int rc = select_with_deadline(m_sockfd, /*read*/true, /*write*/false, timeout_us);
    if (rc <= 0) {
        if (rc == 0) return 0; // timeout
        std::cerr << "[UDP] drain_latest: select(read) failed: " << strerror(errno) << "\n";
        return -1;
    }
    // Drain queue til limits
    const auto t0 = std::chrono::steady_clock::now();
    int got = 0;
    sockaddr_in src{};
    socklen_t slen = sizeof(src);

#ifdef __linux__
    // Fast-path: batch read (up to 16 packets at oncec) - latest-wins
    const int BATCH = 16;
    std::array<mmsghdr, 16> batch{};
    std::array<iovec, 16>   biov{};
#endif
    while (got < max_packets) {
        // time limit
        if (max_us > 0) {
            auto us = std::chrono::duration_cast<std::chrono::microseconds>(
                          std::chrono::steady_clock::now() - t0).count();
            if (us >= max_us) break;
        }

        iovec iov{};
        iov.iov_base = static_cast<void*>(buffer);
        iov.iov_len  = max_len;
        msghdr msg{};
        msg.msg_name    = &src;
        msg.msg_namelen = slen;
        msg.msg_iov     = &iov;
        msg.msg_iovlen  = 1;

        // Nonblocking read(s)
#ifdef __linux__
        // Prepare batch mmsghdr/iovec
        int want = std::min(BATCH, max_packets - got);
        for (int i = 0; i < want; ++i) {
            biov[i].iov_base = static_cast<void*>(buffer);
            biov[i].iov_len  = max_len;
            std::memset(&batch[i], 0, sizeof(mmsghdr));
            batch[i].msg_hdr.msg_name    = &src;
            batch[i].msg_hdr.msg_namelen = sizeof(src);
            batch[i].msg_hdr.msg_iov     = &biov[i];
            batch[i].msg_hdr.msg_iovlen  = 1;
        }
        int nmsgs = ::recvmmsg(m_sockfd, batch.data(), want, MSG_DONTWAIT | MSG_TRUNC, nullptr);
        if (nmsgs < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) break;  // queue empty
            if (errno == EINTR) continue;                        // try again within budget
            std::cerr << "[UDP] drain_latest: recvmmsg failed: " << strerror(errno) << "\n";
            return -1;
        }
        if (nmsgs == 0) break; // nothing to read
        // take last one that is not cut
        for (int i = 0; i < nmsgs; ++i) {
            const msghdr& mh = batch[i].msg_hdr;
            const ssize_t n  = batch[i].msg_len;
            if (n <= 0) continue;
            if (static_cast<size_t>(n) > max_len || (mh.msg_flags & MSG_TRUNC)) {
                // skip cut (latest-wins)
                continue;
            }
            if (m_role == Role::Receiver) { m_peer_addr = src; m_has_peer = true; }
            out_len = static_cast<size_t>(n); // latest-wins: overwrites previous
            ++got;
        }
#else
        ssize_t n = ::recvmsg(m_sockfd, &msg, MSG_TRUNC | MSG_DONTWAIT);
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) break;
            if (errno == EINTR) continue;
            std::cerr << "[UDP] drain_latest: recvmsg failed: " << strerror(errno) << "\n";
            return -1;
        }
        if (static_cast<size_t>(n) > max_len || (msg.msg_flags & MSG_TRUNC)) {
            continue;
        }
        if (m_role == Role::Receiver) { m_peer_addr = src; m_has_peer = true; }
        out_len = static_cast<size_t>(n);
        ++got;
#endif
    }
    if (got == 0) return 0; // nothing in queue
    return 1;
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
    // DSCP (6 bits) goes to TOS << 2 (ECN)
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
