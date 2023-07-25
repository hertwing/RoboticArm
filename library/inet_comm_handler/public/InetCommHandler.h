#ifndef INETCOMMHANDLER_H
#define INETCOMMHANDLER_H

#include <arpa/inet.h>
#include <stdio.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <cstdint>
#include <memory>

class InetCommHandler
{
public:
    explicit InetCommHandler(std::uint64_t buffer_size);
    ~InetCommHandler();

    std::int8_t createTcpServerSocket(const std::uint16_t & port);
    std::int8_t createTcpClientSocket(const std::uint16_t & port, const std::string & server_ip);

    void serverRead(char * buffer);
    void serverWrite(const char * buffer);

    void clientRead(char * buffer);
    void clientWrite(const char * buffer);
private:
    std::uint64_t m_buffer_size;

    int m_sockfd;
    int m_connfd;

    unsigned int m_len;

    struct sockaddr_in m_servaddr;
    struct sockaddr_in m_cli;
};

#endif // INETCOMMHANDLER_H