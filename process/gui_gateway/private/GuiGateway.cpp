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
    m_control_selection.control_selection = static_cast<std::uint8_t>(ControlSelection::NONE);
    m_previous_control_selection.control_selection = static_cast<std::uint8_t>(ControlSelection::NONE);
}

void GuiGateway::runProcess(int argc, char * argv[])
{
    std::string board_name;
    if (std::string(argv[1]).find(ARM_BOARD_NAME) != std::string::npos)
    {
        board_name = ARM_BOARD_NAME;
    }
    else if (std::string(argv[1]).find(GUI_BOARD_NAME) != std::string::npos)
    {
        board_name = GUI_BOARD_NAME;
    }
    else
    {
        std::cout << "Wrong board name passed as the parameter to the process. Exiting." << std::endl;
        return;
    }
    std::cout << "Running GUI gateway process on " << board_name << "." << std::endl;
    board_name == ARM_BOARD_NAME ? runOnArm() : runOnGui();
}

void GuiGateway::runOnGui()
{
<<<<<<< Updated upstream
    // Testing puprposes only. Need to be rewrite
=======
>>>>>>> Stashed changes
    std::cout << "Assigning threads." << std::endl;
    std::cout << "Starting GUI diagnostic thread." << std::endl;
    m_diagnostic_thread = std::thread(handleGuiDiagnostic, this);
    std::cout << "Starting GUI control selection thread." << std::endl;
    m_control_selection_thread = std::thread(handleGuiControlSelection, this);
    m_diagnostic_thread.join();
    m_control_selection_thread.join();
}

void GuiGateway::runOnArm()
{
<<<<<<< Updated upstream
    // Testing puprposes only. Need to be rewrite

=======
>>>>>>> Stashed changes
    std::cout << "Assigning threads." << std::endl;
    std::cout << "Starting ARM diagnostic thread." << std::endl;
    m_diagnostic_thread = std::thread(handleArmDiagnostic, this);
    std::cout << "Starting ARM control selection thread." << std::endl;
    m_control_selection_thread = std::thread(handleArmControlSelection, this);
    m_diagnostic_thread.join();
    m_control_selection_thread.join();
}

void GuiGateway::handleGuiDiagnostic(GuiGateway * gg)
{
<<<<<<< Updated upstream

    gg->m_diagnostic_shmem_handler = std::make_unique<ShmemHandler<DiagnosticData>>(
        odin::shmem_wrapper::DataTypes::DIAGNOSTIC_FROM_REMOTE_SHMEM_NAME, sizeof(DiagnosticData), true);
    gg->m_diagnostic_comm_handler = std::make_unique<InetCommHandler<DiagnosticData>>(
        sizeof(odin::diagnostic_handler::DiagnosticData), DIAGNOSTIC_SOCKET_PORT);
    while (gg->m_run_process)
    {
        // std::cout << "Reading data from server." << std::endl;
        gg->m_diagnostic_comm_handler->serverRead(&(gg->m_remote_diagnostic));
=======
    gg->m_diagnostic_comm_handler = std::make_unique<InetCommHandler<DiagnosticData>>(
        sizeof(odin::diagnostic_handler::DiagnosticData), DIAGNOSTIC_SOCKET_PORT);
    gg->m_diagnostic_shmem_handler = std::make_unique<ShmemHandler<DiagnosticData>>(
        odin::shmem_wrapper::DataTypes::DIAGNOSTIC_FROM_REMOTE_SHMEM_NAME, sizeof(DiagnosticData), true);
    while (gg->m_run_process)
    {
        // std::cout << "Reading data from server." << std::endl;
        std::cout << "Server reads diagnostic from client." << std::endl;
        gg->m_diagnostic_comm_handler->serverRead(&gg->m_remote_diagnostic);
>>>>>>> Stashed changes
        // std::cout << gg->m_remote_diagnostic.cpu_temp << " "
        //         << gg->m_remote_diagnostic.cpu_usage << " "
        //         << gg->m_remote_diagnostic.ram_usage << " "
        //         << gg->m_remote_diagnostic.latency << std::endl;
        // std::cout << "Writing to shmem." << std::endl;
<<<<<<< Updated upstream
        gg->m_diagnostic_shmem_handler->shmemWrite(&(gg->m_remote_diagnostic));
=======
        if (gg->m_previous_remote_diagnostic != gg->m_remote_diagnostic)
        {
            gg->m_diagnostic_shmem_handler->shmemWrite(&gg->m_remote_diagnostic);
            gg->m_previous_remote_diagnostic = gg->m_remote_diagnostic;
        }
>>>>>>> Stashed changes
        // TODO: change magic number to settings constants for sleeps
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
    }
}

void GuiGateway::handleGuiControlSelection(GuiGateway * gg)
{   
<<<<<<< Updated upstream
    gg->m_control_selection_shmem_handler = std::make_unique<ShmemHandler<OdinControlSelection>>(
        odin::shmem_wrapper::DataTypes::CONTROL_SELECT_SHMEM_NAME, sizeof(OdinControlSelection), false);
    gg->m_control_selection_comm_handler = std::make_unique<InetCommHandler<OdinControlSelection>>(
        sizeof(OdinControlSelection), CONTROL_SELECTION_PORT);
=======
    gg->m_control_selection_comm_handler = std::make_unique<InetCommHandler<OdinControlSelection>>(
        sizeof(OdinControlSelection), CONTROL_SELECTION_PORT);
    gg->m_control_selection_shmem_handler = std::make_unique<ShmemHandler<OdinControlSelection>>(
        odin::shmem_wrapper::DataTypes::CONTROL_SELECT_SHMEM_NAME, sizeof(OdinControlSelection), false);
>>>>>>> Stashed changes
    while (gg->m_run_process)
    {
        if (gg->m_control_selection_shmem_handler->openShmem())
        {
<<<<<<< Updated upstream
            if(gg->m_control_selection_shmem_handler->shmemRead(&(gg->m_control_selection)))
            {
                gg->m_control_selection_comm_handler->serverWrite(&(gg->m_control_selection));
=======
            if(gg->m_control_selection_shmem_handler->shmemRead(&gg->m_control_selection))
            {
                if (gg->m_control_selection.control_selection != gg->m_previous_control_selection.control_selection)
                {
                    std::cout << "Writing control to client" << std::endl;
                    gg->m_control_selection_comm_handler->serverWrite(&gg->m_control_selection);
                    gg->m_previous_control_selection.control_selection = gg->m_control_selection.control_selection;
                }
>>>>>>> Stashed changes
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
}

void GuiGateway::handleArmDiagnostic(GuiGateway * gg)
{
<<<<<<< Updated upstream
    gg->m_diagnostic_shmem_handler = std::make_unique<ShmemHandler<DiagnosticData>>(
        odin::shmem_wrapper::DataTypes::DIAGNOSTIC_SHMEM_NAME, sizeof(DiagnosticData), false);
    gg->m_diagnostic_comm_handler = std::make_unique<InetCommHandler<DiagnosticData>>(
        sizeof(odin::diagnostic_handler::DiagnosticData), DIAGNOSTIC_SOCKET_PORT, ROBOTIC_GUI_IP);
=======

    gg->m_diagnostic_comm_handler = std::make_unique<InetCommHandler<DiagnosticData>>(
        sizeof(odin::diagnostic_handler::DiagnosticData), DIAGNOSTIC_SOCKET_PORT, ROBOTIC_GUI_IP);
    gg->m_diagnostic_shmem_handler = std::make_unique<ShmemHandler<DiagnosticData>>(
        odin::shmem_wrapper::DataTypes::DIAGNOSTIC_SHMEM_NAME, sizeof(DiagnosticData), false);
>>>>>>> Stashed changes
    while (gg->m_run_process)
    {
        // std::cout << "Reading from shmem." << std::endl;
        if (gg->m_diagnostic_shmem_handler->openShmem())
        {
<<<<<<< Updated upstream
            if (gg->m_diagnostic_shmem_handler->shmemRead(&(gg->m_remote_diagnostic)))
            {
                std::cout << "Writing diagnostic to server." << std::endl;
                gg->m_diagnostic_comm_handler->clientWrite(&(gg->m_remote_diagnostic));
            }
        }
=======
            if (gg->m_diagnostic_shmem_handler->shmemRead(&gg->m_remote_diagnostic))
            {
                std::cout << "Diagnostic data read from shmem." << std::endl;
                if (gg->m_previous_remote_diagnostic != gg->m_remote_diagnostic)
                {
                    std::cout << "Diagnostic data change. Writing to server." << std::endl;
                    gg->m_diagnostic_comm_handler->clientWrite(&gg->m_remote_diagnostic);
                    gg->m_previous_remote_diagnostic = gg->m_remote_diagnostic;
                    std::cout << "Writing to server end." << std::endl;
                }
            }
        }
        
            
>>>>>>> Stashed changes
        // std::cout << gg->m_remote_diagnostic.cpu_temp << " "
        //     << gg->m_remote_diagnostic.cpu_usage << " "
        //     << gg->m_remote_diagnostic.ram_usage << " "
        //     << gg->m_remote_diagnostic.latency << std::endl;
        // std::cout << "Writing to server." << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
    }
}

void GuiGateway::handleArmControlSelection(GuiGateway * gg)
{
<<<<<<< Updated upstream
    gg->m_control_selection_shmem_handler = std::make_unique<ShmemHandler<OdinControlSelection>>(
        odin::shmem_wrapper::DataTypes::CONTROL_SELECT_SHMEM_NAME, sizeof(OdinControlSelection), true);
    gg->m_control_selection_comm_handler = std::make_unique<InetCommHandler<OdinControlSelection>>(
        sizeof(OdinControlSelection), CONTROL_SELECTION_PORT, ROBOTIC_GUI_IP);
    gg->m_control_selection_shmem_handler->shmemWrite(&(gg->m_control_selection));
    while (gg->m_run_process)
    {
        std::cout << "Reading controlls from server." << std::endl;
        gg->m_control_selection_comm_handler->clientRead(&(gg->m_control_selection));
        gg->m_control_selection_shmem_handler->shmemWrite(&(gg->m_control_selection));
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
=======

    gg->m_control_selection_comm_handler = std::make_unique<InetCommHandler<OdinControlSelection>>(
        sizeof(OdinControlSelection), CONTROL_SELECTION_PORT, ROBOTIC_GUI_IP);
    gg->m_control_selection_shmem_handler = std::make_unique<ShmemHandler<OdinControlSelection>>(
        odin::shmem_wrapper::DataTypes::CONTROL_SELECT_SHMEM_NAME, sizeof(OdinControlSelection), true);
    gg->m_control_selection_shmem_handler->shmemWrite(&gg->m_control_selection);

    while (gg->m_run_process)
    {
        
        gg->m_control_selection_comm_handler->clientRead(&gg->m_control_selection);
        if (gg->m_control_selection.control_selection != gg->m_previous_control_selection.control_selection)
        {
            std::cout << "Reading control from client and writing to shmem" << std::endl;
            gg->m_control_selection_shmem_handler->shmemWrite(&gg->m_control_selection);
            gg->m_previous_control_selection.control_selection = gg->m_control_selection.control_selection;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
>>>>>>> Stashed changes
    }
}

void GuiGateway::signalCallbackHandler(int signum)
{
    std::cout << "GuiGateway received signal: " << signum << std::endl;
    m_run_process = false;
}

} // gui_gateway
} // odin