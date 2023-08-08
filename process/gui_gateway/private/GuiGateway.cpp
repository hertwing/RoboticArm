#include "odin/gui_gateway/GuiGateway.h"
#include "odin/shmem_wrapper/DataTypes.h"
#include "InetCommData.h"

#include <chrono>
#include <thread>
#include <iostream>
#include <signal.h>

using namespace odin::shmem_wrapper;
using namespace odin::diagnostic_handler;

namespace odin
{
namespace gui_gateway
{

bool GuiGateway::m_run_process = true;

GuiGateway::GuiGateway()
{
    
}

void GuiGateway::runProcess(int argc, char * argv[])
{
    std::string(argv[1]).find(ARM_BOARD_NAME) != std::string::npos ? runOnArm() : runOnGui();
    
}

void GuiGateway::runOnGui()
{
    // Testing puprposes only. Need to be rewrite
    m_comm_handler = std::make_unique<InetCommHandler<DiagnosticData>>(
        sizeof(odin::diagnostic_handler::DiagnosticData), SOCKET_PORT);
    m_shmem_handler = std::make_unique<ShmemHandler<DiagnosticData>>(
        odin::shmem_wrapper::DataTypes::DIAGNOSTIC_FROM_REMOTE_SHMEM_NAME, sizeof(DiagnosticData), true);

    while (m_run_process)
    {
        std::cout << "Reading data from server." << std::endl;
        m_comm_handler->serverRead(&m_remote_diagnostic);
        std::cout << m_remote_diagnostic.cpu_temp << " "
                << m_remote_diagnostic.cpu_usage << " "
                << m_remote_diagnostic.ram_usage << " "
                << m_remote_diagnostic.latency << std::endl;
        std::cout << "Writing to shmem." << std::endl;
        m_shmem_handler->shmemWrite(&m_remote_diagnostic);
        
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
}

void GuiGateway::runOnArm()
{
    // Testing puprposes only. Need to be rewrite
    m_comm_handler = std::make_unique<InetCommHandler<DiagnosticData>>(
        sizeof(odin::diagnostic_handler::DiagnosticData), SOCKET_PORT, ROBOTIC_GUI_IP);
    m_shmem_handler = std::make_unique<ShmemHandler<DiagnosticData>>(
        odin::shmem_wrapper::DataTypes::DIAGNOSTIC_SHMEM_NAME, sizeof(DiagnosticData), false);

    while (m_run_process)
    {
        if (m_shmem_handler->openShmem())
        {
            std::cout << "Reading from shmem." << std::endl;
            m_shmem_handler->shmemRead(&m_remote_diagnostic);
            std::cout << m_remote_diagnostic.cpu_temp << " "
                << m_remote_diagnostic.cpu_usage << " "
                << m_remote_diagnostic.ram_usage << " "
                << m_remote_diagnostic.latency << std::endl;
            std::cout << "Writing to server." << std::endl;
            m_comm_handler->clientWrite(&m_remote_diagnostic);
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
}

void GuiGateway::signalCallbackHandler(int signum)
{
    std::cout << "GuiGateway received signal: " << signum << std::endl;
    
    m_run_process = false;
}

} // gui_gateway
} // odin