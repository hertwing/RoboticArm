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
    std::int8_t reconnectClient();

    bool serverRead(T * buffer);
    bool serverWrite(const T * buffer);

    bool clientRead(T * buffer);
    bool clientWrite(const T * buffer);
    static void signalCallbackHandler(int signum);

private:
    bool handleConnection();

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
    m_port(port)
    {
        while(createTcpServer() != 0 && m_run_process)
        {
            std::this_thread::sleep_for(std::chrono::seconds(2));
        };
        while(acceptClient() != 0 && m_run_process)
        {
            std::this_thread::sleep_for(std::chrono::seconds(2));
        };
    };

template <typename T>
InetCommHandler<T>::InetCommHandler(std::uint64_t buffer_size, const std::uint16_t & port, const std::string & server_ip) :
    m_buffer_size(buffer_size),
    m_port(port),
    m_server_ip(server_ip)
    {
        // m_read_timeout.tv_sec = 5;
        // m_read_timeout.tv_usec = 0;
        while(createTcpClientSocket() != 0 && m_run_process)
        {
            std::this_thread::sleep_for(std::chrono::seconds(2));
        };
    };

template <typename T>
InetCommHandler<T>::~InetCommHandler()
{
    close(m_sockfd);
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
        close(m_sockfd);
        return -1;
    }
    
    m_servaddr.sin_family = AF_INET;
    m_servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    m_servaddr.sin_port = htons(m_port);
    int reuse_addr = 1;
    if (setsockopt(m_sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse_addr, sizeof(int)) == -1)
    {
        std::cout << "Error while setting server socket options: " << strerror(errno) << std::endl;
        close(m_sockfd);
        return -1;
    }
    if (bind(m_sockfd, (struct sockaddr*)&m_servaddr, sizeof(m_servaddr)) == -1)
    {
        std::cout << "Binding server socket error: " << strerror(errno) << std::endl;
        close(m_sockfd);
        return -1;
    }
    if (listen(m_sockfd, 5) == -1)
    {
        std::cout << "Server listen error: " << strerror(errno) << std::endl;
        close(m_sockfd);
        return -1;
    }
    std::cout << "Server created." << std::endl;
    return 0;
}

template <typename T>
std::int8_t InetCommHandler<T>::acceptClient()
{
    if (!handleConnection())
    {
        return 0;
    }
    std::cout << "Accepting client." << std::endl;
    m_connfd = accept(m_sockfd, NULL, NULL);
    if (m_connfd == -1)
    {
        std::cout << "Error when accepting client: " << strerror(errno) << std::endl;
        return -1;
    }
    std::cout << "Client accepted." << std::endl;
    return 0;
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
        close(m_sockfd);
        return -1;
    }

    m_servaddr.sin_family = AF_INET;
    m_servaddr.sin_port = htons(m_port);
    m_servaddr.sin_addr.s_addr = inet_addr(m_server_ip.c_str());

    if (connect(m_sockfd, (struct sockaddr*)&m_servaddr, sizeof(m_servaddr)) == -1)
    {
        std::cout << "Error when connecting to the server: " << strerror(errno) << std::endl;
        close(m_sockfd);
        return -1;
    }

    std::cout << "Connected with the server." << std::endl;
    return 0;
}

template <typename T>
bool InetCommHandler<T>::serverRead(T * buff)
{
    if (!handleConnection())
    {
        return 0;
    }
    fd_set readSet;
    struct timeval timeout;

    FD_ZERO(&readSet);
    FD_SET(m_connfd, &readSet);

    timeout.tv_sec = 2;
    timeout.tv_usec = 0;

    int result = select(m_connfd + 1, &readSet, NULL, NULL, &timeout);
    if (result == -1)
    {
        std::cout << "Error during select when reading message: " << strerror(errno) << std::endl;
        close(m_connfd);
        while(acceptClient() != 0)
        {
            std::this_thread::sleep_for(std::chrono::seconds(2));
        };
        return 0;
    }
    else if (result == 0)
    {
        return 0;
    }
    result = recv(m_connfd, buff, m_buffer_size, MSG_DONTWAIT);
    if (result <= 0)
    {
        std::cout << "Error when reading from client: " << strerror(errno) << std::endl;
        close(m_connfd);
        while(acceptClient() != 0)
        {
            std::this_thread::sleep_for(std::chrono::seconds(2));
        };
        return 0;
    }
    return 1;
}

template <typename T>
bool InetCommHandler<T>::serverWrite(const T * buff)
{
    if (!handleConnection())
    {
        return 0;
    }
    fd_set writeSet;
    struct timeval timeout;

    FD_ZERO(&writeSet);
    FD_SET(m_connfd, &writeSet);

    timeout.tv_sec = 2;
    timeout.tv_usec = 0;

    int result = select(m_connfd + 1, NULL, &writeSet, NULL, &timeout);
    if (result == -1)
    {
        std::cout << "Error during select when writing message: " << strerror(errno) << std::endl;
        close(m_connfd);
        while(acceptClient() != 0)
        {
            std::this_thread::sleep_for(std::chrono::seconds(2));
        };
        return 0;
    }
    else if (result == 0)
    {
        return 0;
    }
    result = send(m_connfd, buff, m_buffer_size, MSG_NOSIGNAL);
    if (result == -1)
    {
        std::cout << "Error when writing to client: " << strerror(errno) << std::endl;
        close(m_connfd);
        while(acceptClient() != 0)
        {
            std::this_thread::sleep_for(std::chrono::seconds(2));
        };
        return 0;
    }
    return 1;
}

template <typename T>
bool InetCommHandler<T>::clientRead(T * buff)
{
    if (!handleConnection())
    {
        return 0;
    }
    fd_set readSet;
    struct timeval timeout;

    FD_ZERO(&readSet);
    FD_SET(m_sockfd, &readSet);

    timeout.tv_sec = 2;
    timeout.tv_usec = 0;

    int result = select(m_sockfd + 1, &readSet, NULL, NULL, &timeout);
    if (result == -1)
    {
        std::cout << "Error during select when reading message: " << strerror(errno) << std::endl;
        close(m_sockfd);
        while(createTcpClientSocket() != 0)
        {
            std::this_thread::sleep_for(std::chrono::seconds(2));
        };
        return 0;
    }
    else if (result == 0)
    {
        return 0;
    }
    result = recv(m_sockfd, buff, m_buffer_size, MSG_DONTWAIT);
    if (result <= 0)
    {
        std::cout << "Error when reading from server: " << strerror(errno) << std::endl;
        close(m_sockfd);
        while(createTcpClientSocket() != 0)
        {
            std::this_thread::sleep_for(std::chrono::seconds(2));
        };
        return 0;
    }
    return 1;
}

template <typename T>
bool InetCommHandler<T>::clientWrite(const T * buff)
{
    if (!handleConnection())
    {
        return 0;
    }
    fd_set writeSet;
    struct timeval timeout;

    FD_ZERO(&writeSet);
    FD_SET(m_sockfd, &writeSet);

    timeout.tv_sec = 2;
    timeout.tv_usec = 0;

    int result = select(m_sockfd + 1, NULL, &writeSet, NULL, &timeout);
    if (result == -1)
    {
        std::cout << "Error during select when writing message: " << strerror(errno) << std::endl;
        close(m_sockfd);
        while(acceptClient() != 0)
        {
            std::this_thread::sleep_for(std::chrono::seconds(2));
        };
        return 0;
    }
    else if (result == 0)
    {
        return 0;
    }
    result = send(m_sockfd, buff, m_buffer_size, MSG_NOSIGNAL);
    if (result == -1)
    {
        std::cout << "Error when writing to server: " << strerror(errno) << std::endl;
        close(m_sockfd);
        while(createTcpClientSocket() != 0)
        {
            std::this_thread::sleep_for(std::chrono::seconds(2));
        };
        return 0;
    }
    return 1;
}

template <typename T>
bool InetCommHandler<T>::handleConnection()
{
    if (!m_run_process)
    {
        if (fcntl(m_sockfd, F_GETFD) != -1 || errno != EBADF)
        {
            std::cout << "Closing connection socket." << std::endl;
            close(m_sockfd);
        }
        if (fcntl(m_connfd, F_GETFD) != -1 || errno != EBADF)
        {
            std::cout << "Closing client socket." << std::endl;
            close(m_connfd);
        }
        return false;
    }
    return true;
}

template <typename T>
void InetCommHandler<T>::signalCallbackHandler(int signum)
{
    std::cout << "InetCommHandler received signal: " << signum << std::endl;
    m_run_process = false;
}

#endif // INETCOMMHANDLER_H