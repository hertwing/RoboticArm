#include "InetCommHandler.h"
#include "InetCommData.h"

#include <chrono>
#include <thread>
#include <iostream>

#define MAX_BUFF 1024

int main()
{
    InetCommHandler inet_comm_handler(MAX_BUFF);
    inet_comm_handler.createTcpClientSocket(7021, ROBOTIC_GUI_IP);

    char buff[MAX_BUFF];

    while(true)
    {
        char * msg = "Kisses from client!";
        inet_comm_handler.clientWrite(msg);
        std::this_thread::sleep_for(std::chrono::seconds(3));
        inet_comm_handler.clientRead(buff);
        std::cout << buff << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(3));
    }

    return 0;
}