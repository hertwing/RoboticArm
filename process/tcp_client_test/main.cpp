#include "InetCommHandler.hpp"
#include "InetCommData.h"
#include "odin/diagnostic_handler/DataTypes.h"

#include <chrono>
#include <thread>
#include <iostream>
#include <signal.h>

#define MAX_BUFF 1024

int main()
{
    signal(SIGPIPE, SIG_IGN);
    InetCommHandler<odin::diagnostic_handler::DiagnosticData> inet_comm_handler(sizeof(odin::diagnostic_handler::DiagnosticData), 7073, "192.168.1.41");

    odin::diagnostic_handler::DiagnosticData dd_send;
    odin::diagnostic_handler::DiagnosticData dd_rec;

    dd_send.cpu_temp = 15;
    dd_send.cpu_usage = 55;
    dd_send.ram_usage = 35;
    dd_send.latency = 5.65;

    while(true)
    {
        const char * msg = "Kisses from client!";
        inet_comm_handler.clientWrite(&dd_send);
        std::this_thread::sleep_for(std::chrono::seconds(3));
        inet_comm_handler.clientRead(&dd_rec);
        std::cout << dd_rec.cpu_temp << " " << dd_rec.cpu_usage << " " << dd_rec.ram_usage << " " << dd_rec.latency << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(3));
    }

    return 0;
}