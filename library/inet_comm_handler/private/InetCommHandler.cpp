#include "InetCommHandler.h"

#include <cstring>
#include <iostream>

InetCommHandler::InetCommHandler(std::uint64_t buffer_size) : m_buffer_size(buffer_size){};

InetCommHandler::~InetCommHandler()
{
    close(m_sockfd);
}

std::int8_t InetCommHandler::createTcpServerSocket(const std::uint16_t & port)
{
    std::cout << "Initializing TCP server socket for port: " << port << std::endl;
    // socket create and verification
    m_sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (m_sockfd == -1)
    {
        std::cerr << "Socket creation failed." << std::endl;
        return -1;
    }
    else
    {
        std::cout << "Socket successfully created." << std::endl;
    }
//    bzero(&m_servaddr, sizeof(m_servaddr));
    memset(&m_servaddr, sizeof(m_servaddr), 0);

    // assign IP, PORT
    m_servaddr.sin_family = AF_INET;
    m_servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    m_servaddr.sin_port = htons(port);

    // Binding newly created socket to given IP and verification
    if ((bind(m_sockfd, (struct sockaddr*)&m_servaddr, sizeof(m_servaddr))) != 0)
    {
        std::cerr << "Socket bind failed." << std::endl;
        return -1;
    }
    else
    {
        std::cout << "Socket successfully binded." << std::endl;
    }

    // Now server is ready to listen and verification
    if ((listen(m_sockfd, 5)) != 0)
    {
        std::cerr << "Server listen failed." << std::endl;
        return -1;
    }
    else
    {
        std::cout << "Server listening." << std::endl;
    }
    m_len = sizeof(m_cli);
   
    // Accept the data packet from m_client and verification
    m_connfd = accept(m_sockfd, (struct sockaddr*)&m_cli, &m_len);
    if (m_connfd < 0)
    {
        std::cerr << "Server accept failed." << std::endl;
        exit(0);
    }
    else
    {
        std::cout << "Server accepted the m_client." << std::endl;
    }
    return 0;
}

std::int8_t InetCommHandler::createTcpClientSocket(const std::uint16_t & port, const std::string & server_ip)
{
    std::cout << "Initializing TCP m_client socket for server IP: " << server_ip << ", port: " << port << std::endl;
    // socket create and verification
    m_sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (m_sockfd == -1)
    {
        std::cerr << "Socket creation failed." << std::endl;
        return -1;
    }
    else
    {
        std::cout << "Socket successfully created." << std::endl;
    }
    memset(&m_servaddr, sizeof(m_servaddr), 0);
//    bzero(&m_servaddr, sizeof(m_servaddr));
 
    // assign IP, PORT
    m_servaddr.sin_family = AF_INET;
    m_servaddr.sin_addr.s_addr = inet_addr(server_ip.c_str());
    m_servaddr.sin_port = htons(port);
 
    // connect the m_client socket to server socket
    if (connect(m_sockfd, (struct sockaddr*)&m_servaddr, sizeof(m_servaddr)) != 0)
    {
        std::cerr << "Connection with the server failed.\n" << std::endl;
        return -1;
    }
    else
    {
        std::cout << "Connected to the server." << std::endl;
    }
    return 0;
}

void InetCommHandler::serverRead(char * buff)
{
    memset(buff, m_buffer_size, 0);
//    bzero(buff, m_buffer_size);
    read(m_connfd, buff, m_buffer_size);
}

void InetCommHandler::serverWrite(const char * buff)
{
    write(m_connfd, buff, m_buffer_size);
}

void InetCommHandler::clientRead(char * buff)
{
    memset(buff, sizeof(m_buffer_size), 0);
//    bzero(buff, m_buffer_size);
    read(m_sockfd, buff, m_buffer_size);
}

void InetCommHandler::clientWrite(const char * buff)
{
    write(m_sockfd, buff, m_buffer_size);
}
