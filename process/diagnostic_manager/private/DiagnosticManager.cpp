#include "odin/diagnostic_manager/DiagnosticManager.h"
#include "odin/diagnostic_handler/DataTypes.h"

#include <chrono>
#include <thread>
#include <iostream>
#include <signal.h>

using namespace odin::diagnostic_handler;

namespace odin
{
namespace diagnostic_manager
{

bool DiagnosticManager::m_run_process = true;

DiagnosticManager::DiagnosticManager() :
    m_diagnostic_handler()
{
}

void DiagnosticManager::diagnosticReader()
{
    while (m_run_process)
    {
        m_diagnostic_handler.writeDiagnostic(m_board_name);
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
    }
}

void DiagnosticManager::runProcess(int argc, char * argv[])
{
    std::string(argv[1]).find(ARM_BOARD_NAME) != std::string::npos ? m_board_name = ARM_BOARD_NAME : m_board_name = GUI_BOARD_NAME;
    diagnosticReader();
}

void DiagnosticManager::signalCallbackHandler(int signum)
{
    DiagnosticManager::signalCallbackHandler(signum);
    std::cout << "DiagnosticManager received signal: " << signum << std::endl;

    m_run_process = false;
}

} // diagnostic_manager
} // odin