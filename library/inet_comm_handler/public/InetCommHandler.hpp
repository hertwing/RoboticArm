#ifndef INETCOMMHANDLER_H
#define INETCOMMHANDLER_H

#include <arpa/inet.h>
#include <fcntl.h>
#include <stdio.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <cstdint>
#include <cstring>
#include <iostream>
#include <memory>
#include <thread>
#include <chrono>

template <typename T>
class InetCommHandler
{
public:
    InetCommHandler(std::uint64_t buffer_size, const std::uint16_t & port);
    InetCommHandler(std::uint64_t buffer_size, const std::uint16_t & port, const std::string & server_ip);
    ~InetCommHandler();

    std::int8_t createTcpServer();
    std::int8_t acceptClient();
    std::int8_t createTcpClientSocket();

    std::int8_t serverRead(T * buffer);
    bool serverWrite(const T * buffer);

    std::int8_t clientRead(T * buffer);
    bool clientWrite(const T * buffer);
    static void signalCallbackHandler(int signum);

private:
    void disconnectAndWaitForNewClient();
    void reconnectToServer();

    bool handleConnection();

    int  select_with_deadline(int fd, bool want_read, bool want_write,
                              int timeout_us) const;
    bool read_exact(int fd, void* buf, size_t len, int timeout_us);
    bool write_exact(int fd, const void* buf, size_t len, int timeout_us);
    void enable_tcp_options(int fd); // keepalive/nodelay
private:
    std::uint64_t m_buffer_size;

    int m_sockfd;
    int m_connfd;
    std::uint16_t m_port;
    std::string m_server_ip;

    unsigned int m_len;

    struct sockaddr_in m_servaddr;
    struct sockaddr_in m_cli;

    fd_set m_read_set;
    int m_server_activity;
    struct timeval m_read_timeout;

    static bool m_run_process;
};

template <typename T>
bool InetCommHandler<T>::m_run_process = true;

template <typename T>
InetCommHandler<T>::InetCommHandler(std::uint64_t buffer_size, const std::uint16_t & port) :
    m_buffer_size(buffer_size),
    m_sockfd(-1),
    m_connfd(-1),
    m_port(port),
    m_server_ip(""),
    m_len(0),
    m_servaddr{},
    m_cli{},
    m_read_set{},
    m_server_activity(0),
    m_read_timeout{0,0}
{
    while(createTcpServer() != 0 && m_run_process)
    {
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }
    while(acceptClient() != 0 && m_run_process)
    {
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }
};

template <typename T>
InetCommHandler<T>::InetCommHandler(std::uint64_t buffer_size, const std::uint16_t & port, const std::string & server_ip) :
    m_buffer_size(buffer_size),
    m_port(port),
    m_server_ip(server_ip)
{
    while(createTcpClientSocket() != 0 && m_run_process)
    {
        std::this_thread::sleep_for(std::chrono::seconds(2));
    };
};

template <typename T>
InetCommHandler<T>::~InetCommHandler()
{
    if (m_sockfd >= 0) close(m_sockfd);
    if (m_connfd >= 0) close(m_connfd);
    m_sockfd = -1;
    m_connfd = -1;
}

template <typename T>
std::int8_t InetCommHandler<T>::createTcpServer()
{
    if (!handleConnection())
    {
        return 0;
    }
    m_sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (m_sockfd == -1)
    {
        std::cout << "Failed to create server socket." << std::endl;
        return -1;
    }

    std::memset(&m_servaddr, 0, sizeof(m_servaddr));
    m_servaddr.sin_family = AF_INET;
    m_servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    m_servaddr.sin_port = htons(m_port);

    int reuse_addr = 1;
    if (setsockopt(m_sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse_addr, sizeof(reuse_addr)) == -1) {
        std::cout << "Error setting SO_REUSEADDR: " << strerror(errno) << std::endl;
        close(m_sockfd);
        m_sockfd = -1;
        return -1;
    }

    int reuse_port = 1;
    if (setsockopt(m_sockfd, SOL_SOCKET, SO_REUSEPORT, &reuse_port, sizeof(reuse_port)) == -1) {
        if (errno != ENOPROTOOPT) {
            std::cout << "Error setting SO_REUSEPORT: " << strerror(errno) << std::endl;
            close(m_sockfd);
            m_sockfd = -1;
            return -1;
        }
    }

    if (bind(m_sockfd, (struct sockaddr*)&m_servaddr, sizeof(m_servaddr)) == -1)
    {
        if (errno == EADDRINUSE)
        {
            std::cout << "Port " << m_port << " already in use. "
                      << "Make sure no other process is using this port." << std::endl;
        }
        else
        {
            std::cout << "Binding server socket error: " << strerror(errno) << std::endl;
        }

        close(m_sockfd);
        m_sockfd = -1;
        return -1;
    }

    if (listen(m_sockfd, 5) == -1)
    {
        std::cout << "Server listen error: " << strerror(errno) << std::endl;
        close(m_sockfd);
        m_sockfd = -1;
        return -1;
    }

    struct sockaddr_in actual_addr;
    char hostbuffer[256];
    if (gethostname(hostbuffer, sizeof(hostbuffer)) == 0) {
        struct hostent *host_entry = gethostbyname(hostbuffer);
        if (host_entry && host_entry->h_addrtype == AF_INET)
        {
            struct sockaddr_in actual_addr;
            socklen_t len = sizeof(actual_addr);
            if (getsockname(m_sockfd, (struct sockaddr*)&actual_addr, &len) == 0) {
                char ip[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &actual_addr.sin_addr, ip, sizeof(ip));
                std::cout << "Server listening on " << ip << ":" << ntohs(actual_addr.sin_port) << std::endl;
            }
        }
    }
    else
    {
        std::cout << "gethostname() failed: " << strerror(errno) << std::endl;
    }

    std::cout << "TCP server created." << std::endl;
    return 0;
}

template <typename T>
std::int8_t InetCommHandler<T>::acceptClient()
{
    if (!handleConnection())
    {
        return 0;
    }

    std::cout << "Accepting client..." << std::endl;

    if (m_sockfd < 0)
    {
        std::cerr << "[ERROR] Cannot accept client: server socket is invalid." << std::endl;
        return -1;
    }

    while (m_run_process)
    {
        m_connfd = accept(m_sockfd, NULL, NULL);
        if (m_connfd == -1)
        {
            if (errno == EINTR)
            {
                std::cout << "Accept interrupted by signal, retrying..." << std::endl;
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
                continue;
            }
            else if (errno == EBADF || errno == EINVAL)
            {
                std::cerr << "Socket invalid or closed. Attempting to recreate server socket..." << std::endl;
                close(m_sockfd);
                m_sockfd = -1;
                if (createTcpServer() != 0)
                {
                    std::cerr << "Recreating server socket failed." << std::endl;
                    return -1;
                }
                continue;
            }
            std::cout << "Error when accepting client: " << strerror(errno) << std::endl;
            return -1;
        }

        std::cout << "Client accepted." << std::endl;
        return 0;
    }

    std::cout << "Server stopped, cannot accept clients." << std::endl;
    return -1;
}

template <typename T>
std::int8_t InetCommHandler<T>::createTcpClientSocket()
{
    if (!handleConnection())
    {
        return 0;
    }
    std::cout << "Creating tcp client socket." << std::endl;
    m_sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (m_sockfd == -1)
    {
        std::cout << "Error when creating client socket: " << strerror(errno) << std::endl;
        return -1;
    }

    m_servaddr.sin_family = AF_INET;
    m_servaddr.sin_port = htons(m_port);
    if (inet_pton(AF_INET, m_server_ip.c_str(), &m_servaddr.sin_addr) <= 0)
    {
        std::cout << "Invalid address or address not supported: " << m_server_ip << std::endl;
        close(m_sockfd);
        m_sockfd = -1;
        return -1;
    }

    int reuse_addr = 1;
    if (setsockopt(m_sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse_addr, sizeof(reuse_addr)) == -1) {
        std::cout << "Error setting SO_REUSEADDR: " << strerror(errno) << std::endl;
        close(m_sockfd);
        m_sockfd = -1;
        return -1;
    }

    int reuse_port = 1;
    if (setsockopt(m_sockfd, SOL_SOCKET, SO_REUSEPORT, &reuse_port, sizeof(reuse_port)) == -1) {
        if (errno != ENOPROTOOPT) {
            std::cout << "Error setting SO_REUSEPORT: " << strerror(errno) << std::endl;
            close(m_sockfd);
            m_sockfd = -1;
            return -1;
        }
    }

    if (connect(m_sockfd, (struct sockaddr*)&m_servaddr, sizeof(m_servaddr)) == -1)
    {
        std::cout << "Error when connecting to the server: " << strerror(errno) << std::endl;
        close(m_sockfd);
        m_sockfd = -1;
        return -1;
    }

    std::cout << "Connected with the server." << std::endl;
    return 0;
}

template <typename T>
std::int8_t InetCommHandler<T>::serverRead(T * buff)
{
    if (!handleConnection())
    {
        return -1;
    }
    if (!read_exact(m_connfd, buff, m_buffer_size, /*100ms*/100000)) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) return 0; // timeout
        std::cerr << "[server] read_exact failed: " << strerror(errno) << std::endl;
        disconnectAndWaitForNewClient();
        return -1;
    }
    return 1;
}

template <typename T>
bool InetCommHandler<T>::serverWrite(const T * buff)
{
    if (!handleConnection())
    {
        return false;
    }
    if (!write_exact(m_connfd, buff, m_buffer_size, 100000)) {
        std::cerr << "[server] write_exact failed: " << strerror(errno) << std::endl;
        disconnectAndWaitForNewClient();
        return false;
    }
    return true;
}

template <typename T>
std::int8_t InetCommHandler<T>::clientRead(T * buff)
{
    if (!handleConnection())
    {
        return -1;
    }
    if (!read_exact(m_sockfd, buff, m_buffer_size, 100000)) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) return 0;
        std::cerr << "[client] read_exact failed: " << strerror(errno) << std::endl;
        reconnectToServer();
        return -1;
    }
    return 1;
}

template <typename T>
bool InetCommHandler<T>::clientWrite(const T * buff)
{
    if (!handleConnection())
    {
        return false;
    }
    if (!write_exact(m_sockfd, buff, m_buffer_size, 100000)) {
        std::cerr << "[client] write_exact failed: " << strerror(errno) << std::endl;
        reconnectToServer();
        return false;
    }
    return true;
}

template <typename T>
bool InetCommHandler<T>::handleConnection()
{
    if (!m_run_process)
    {
        if (m_sockfd >= 0)
        {
            std::cout << "Closing server socket." << std::endl;
            close(m_sockfd);
            m_sockfd = -1;
        }
        if (m_connfd >= 0)
        {
            std::cout << "Closing client socket." << std::endl;
            close(m_connfd);
            m_connfd = -1;
        }
        return false;
    }
    return true;
}

template <typename T>
void InetCommHandler<T>::disconnectAndWaitForNewClient()
{
    if (m_connfd >= 0) { shutdown(m_connfd, SHUT_RDWR); close(m_connfd); }
    m_connfd = -1;
    while (acceptClient() != 0 && m_run_process)
    {
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }
}

template <typename T>
void InetCommHandler<T>::reconnectToServer()
{
    std::cout << "[client] RECONNECTING TO SERVER..." << std::endl;
    if (m_sockfd >= 0) { shutdown(m_sockfd, SHUT_RDWR); close(m_sockfd); }
    m_sockfd = -1;
    while(createTcpClientSocket() != 0 && m_run_process)
    {
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }
}

template <typename T>
void InetCommHandler<T>::signalCallbackHandler(int signum)
{
    std::cout << "InetCommHandler received signal: " << signum << std::endl;
    m_run_process = false;
}

template <typename T>
int InetCommHandler<T>::select_with_deadline(int fd, bool want_read, bool want_write, int timeout_us) const
{
    using clock = std::chrono::steady_clock;
    const auto deadline = clock::now() + std::chrono::microseconds(timeout_us);

    for (;;)
    {
        fd_set rset, wset;
        fd_set *r = nullptr, *w = nullptr;
        if (want_read)  { FD_ZERO(&rset); FD_SET(fd, &rset); r = &rset; }
        if (want_write) { FD_ZERO(&wset); FD_SET(fd, &wset); w = &wset; }

        auto now = clock::now();
        if (now >= deadline) return 0; // timeout
        auto left = std::chrono::duration_cast<std::chrono::microseconds>(deadline - now).count();
        timeval tv { left / 1000000, static_cast<suseconds_t>(left % 1000000) };

        int rc = select(fd + 1, r, w, nullptr, &tv);
        if (rc == -1 && errno == EINTR) continue;
        return rc;
    }
}

template <typename T>
bool InetCommHandler<T>::read_exact(int fd, void* buf, size_t len, int timeout_us)
{
    auto* p = static_cast<char*>(buf);
    size_t got = 0;

    while (got < len)
    {
        int rc = select_with_deadline(fd, /*read*/true, /*write*/false, timeout_us);
        if (rc <= 0) { if (rc == 0) errno = EAGAIN; return false; } // timeout or error

        ssize_t n = recv(fd, p + got, len - got, 0);
        if (n == 0) { errno = ECONNRESET; return false; }
        if (n < 0) {
            if (errno == EINTR) continue;
            if (errno == EAGAIN || errno == EWOULDBLOCK) continue;
            return false;
        }
        got += size_t(n);
    }
    return true;
}

template <typename T>
bool InetCommHandler<T>::write_exact(int fd, const void* buf, size_t len, int timeout_us)
{
    auto* p = static_cast<const char*>(buf);
    size_t sent_total = 0;

    while (sent_total < len)
    {
        int rc = select_with_deadline(fd, /*read*/false, /*write*/true, timeout_us);
        if (rc <= 0) { if (rc == 0) errno = EAGAIN; return false; } // timeout or error

        ssize_t n = send(fd, p + sent_total, len - sent_total, MSG_NOSIGNAL);
        if (n < 0) {
            if (errno == EINTR) continue;
            if (errno == EAGAIN || errno == EWOULDBLOCK) continue;
            return false;
        }
        sent_total += size_t(n);
    }
    return true;
}

template <typename T>
void InetCommHandler<T>::enable_tcp_options(int fd)
{
    // KEEPALIVE
    int ka = 1;
    setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &ka, sizeof(ka));
#ifdef TCP_KEEPIDLE
    int idle = 30;  setsockopt(fd, IPPROTO_TCP, TCP_KEEPIDLE,  &idle, sizeof(idle));
#endif
#ifdef TCP_KEEPINTVL
    int intvl = 10; setsockopt(fd, IPPROTO_TCP, TCP_KEEPINTVL, &intvl, sizeof(intvl));
#endif
#ifdef TCP_KEEPCNT
    int cnt = 3;    setsockopt(fd, IPPROTO_TCP, TCP_KEEPCNT,   &cnt, sizeof(cnt));
#endif
#ifdef TCP_NODELAY
    int nd = 1; setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &nd, sizeof(nd));
#endif
}

#endif // INETCOMMHANDLER_H