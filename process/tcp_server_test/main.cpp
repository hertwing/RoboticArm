#include "InetCommHandler.h"
#include "InetCommData.h"

#include <chrono>
#include <thread>
#include <iostream>

#define MAX_BUFF 1024

int main()
{
    InetCommHandler inet_comm_handler(MAX_BUFF);
    inet_comm_handler.createTcpServerSocket(7021);

    char buff[MAX_BUFF];

    while(true)
    {
        inet_comm_handler.serverRead(buff);
        std::cout << buff << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(3));
        char * msg = "Oh, thank you!";
        inet_comm_handler.serverWrite(msg);
        std::this_thread::sleep_for(std::chrono::seconds(3));
    }
    return 0;
}