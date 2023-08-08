#include "InetCommHandler.hpp"
#include "InetCommData.h"
#include "odin/diagnostic_handler/DataTypes.h"

#include <chrono>
#include <thread>
#include <iostream>
#include <string>

#define MAX_BUFF 1024

int main()
{
    InetCommHandler<odin::diagnostic_handler::DiagnosticData> inet_comm_handler(sizeof(odin::diagnostic_handler::DiagnosticData), 7073);
    // InetCommHandler<char> inet_comm_handler(MAX_BUFF);

    odin::diagnostic_handler::DiagnosticData dd_send;
    odin::diagnostic_handler::DiagnosticData dd_rec;

    dd_send.cpu_temp = 10;
    dd_send.cpu_usage = 50;
    dd_send.ram_usage = 30;
    dd_send.latency = 5.61;

    int count = 0;

    char buff[MAX_BUFF];


    while(true)
    {
        // std::string msg = "Oh, hi Jenny! " + std::to_string(++count);
        inet_comm_handler.serverRead(&dd_rec);
        std::cout << dd_rec.cpu_temp << " " << dd_rec.cpu_usage << " " << dd_rec.ram_usage << " " << dd_rec.latency << std::endl;
        // std::cout << buff << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(3));
        inet_comm_handler.serverWrite(&dd_send);
        std::this_thread::sleep_for(std::chrono::seconds(3));
    }
    return 0;
}