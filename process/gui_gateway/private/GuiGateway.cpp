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

GuiGateway::GuiGateway() :
    m_automatic_movement_status(AutomaticMovementStatus::NONE)
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
    std::cout << "Assigning threads." << std::endl;
    std::cout << "Starting GUI diagnostic thread." << std::endl;
    m_diagnostic_thread = std::thread(handleGuiDiagnostic, this);
    std::cout << "Starting GUI control selection thread." << std::endl;
    m_control_selection_thread = std::thread(handleGuiControlSelection, this);
    std::cout << "Starting automatic data thread." << std::endl;
    m_automatic_data_thread = std::thread(handleGuiAutomaticSteps, this);
    m_diagnostic_thread.join();
    m_control_selection_thread.join();
    m_automatic_data_thread.join();
}

void GuiGateway::runOnArm()
{
    std::cout << "Assigning threads." << std::endl;
    std::cout << "Starting ARM diagnostic thread." << std::endl;
    m_diagnostic_thread = std::thread(handleArmDiagnostic, this);
    std::cout << "Starting ARM control selection thread." << std::endl;
    m_control_selection_thread = std::thread(handleArmControlSelection, this);
    std::cout << "Starting automatic data thread." << std::endl;
    m_automatic_data_thread = std::thread(handleArmAutomaticSteps, this);
    m_diagnostic_thread.join();
    m_control_selection_thread.join();
}

void GuiGateway::handleGuiDiagnostic(GuiGateway * gg)
{

    gg->m_diagnostic_shmem_handler = std::make_unique<ShmemHandler<DiagnosticData>>(
        odin::shmem_wrapper::DataTypes::DIAGNOSTIC_FROM_REMOTE_SHMEM_NAME, sizeof(DiagnosticData), true);
    gg->m_diagnostic_comm_handler = std::make_unique<InetCommHandler<DiagnosticData>>(
        sizeof(odin::diagnostic_handler::DiagnosticData), DIAGNOSTIC_SOCKET_PORT);
    while (gg->m_run_process)
    {
        gg->m_diagnostic_comm_handler->serverRead(&(gg->m_remote_diagnostic));
        if (gg->m_remote_diagnostic != gg->m_previous_remote_diagnostic)
        {
            gg->m_diagnostic_shmem_handler->shmemWrite(&(gg->m_remote_diagnostic));
            gg->m_previous_remote_diagnostic = gg->m_remote_diagnostic;
        }
        
        // TODO: change magic number to settings const for sleeps
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
    }
}

void GuiGateway::handleGuiControlSelection(GuiGateway * gg)
{   
    gg->m_control_selection_shmem_handler = std::make_unique<ShmemHandler<OdinControlSelection>>(
        odin::shmem_wrapper::DataTypes::CONTROL_SELECT_SHMEM_NAME, sizeof(OdinControlSelection), false);
    gg->m_control_selection_comm_handler = std::make_unique<InetCommHandler<OdinControlSelection>>(
        sizeof(OdinControlSelection), CONTROL_SELECTION_PORT);
    while (gg->m_run_process)
    {
        if (gg->m_control_selection_shmem_handler->openShmem())
        {
            if(gg->m_control_selection_shmem_handler->shmemRead(&(gg->m_control_selection)))
            {
                gg->m_control_selection_comm_handler->serverWrite(&(gg->m_control_selection));
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}

void GuiGateway::handleGuiAutomaticSteps(GuiGateway * gg)
{
    gg->m_automatic_execute_shmem_handler = std::make_unique<odin::shmem_wrapper::ShmemHandler<automatic_movement_status_t>>(
        odin::shmem_wrapper::DataTypes::AUTOMATIC_EXECUTE_SHMEM_NAME, sizeof(automatic_movement_status_t), false);
    gg->m_automatic_step_shmem_handler = std::make_unique<odin::shmem_wrapper::ShmemHandler<OdinServoStep>>(
        odin::shmem_wrapper::DataTypes::AUTOMATIC_STEP_SHMEM_NAME, sizeof(OdinServoStep), false);

    gg->m_automatic_execute_comm_handler = std::make_unique<InetCommHandler<automatic_movement_status_t>>(
        sizeof(automatic_movement_status_t), AUTOMATIC_EXECUTION_PORT);
    gg->m_automatic_step_comm_handler = std::make_unique<InetCommHandler<OdinServoStep>>(
        sizeof(OdinServoStep), AUTOMATIC_SERVO_STEP_PORT);

    OdinServoStep servo_step;

    bool is_first_message = true;
    
    while (gg->m_run_process)
    {
        if (gg->m_automatic_execute_shmem_handler->openShmem())
        {
            if (gg->m_automatic_execute_shmem_handler->shmemRead(&gg->m_automatic_movement_status))
            {
                if (gg->m_automatic_movement_status == AutomaticMovementStatus::START_SENDING)
                {
                    // Read data to send
                    if (gg->m_automatic_step_shmem_handler->shmemRead(&servo_step))
                    {
                        if (is_first_message)
                        {
                           is_first_message = false;
                        }

                        std::cout << "Sending automatic data." << std::endl;
                        // TODO: Add error handle
                        gg->m_automatic_execute_comm_handler->serverWrite(&gg->m_automatic_movement_status);
                        gg->m_automatic_step_comm_handler->serverWrite(&servo_step);

                        // Write back sending status to shmem
                        gg->m_automatic_movement_status = AutomaticMovementStatus::SEND_SUCCESS;
                        std::cout << "Writing status to shmem: " << +(gg->m_automatic_movement_status) << std::endl;
                        gg->m_automatic_execute_shmem_handler->shmemWrite(&gg->m_automatic_movement_status);
                    }
                }
                else if (gg->m_automatic_movement_status == AutomaticMovementStatus::SEND_DONE && !is_first_message)
                {
                    std::cout << "Received SEND_DONE message" << std::endl;
                    gg->m_automatic_execute_comm_handler->serverWrite(&gg->m_automatic_movement_status);
                    is_first_message = true;
                    while(true)
                    {
                        gg->m_automatic_execute_comm_handler->serverRead(&gg->m_automatic_movement_status);
                        if (gg->m_automatic_movement_status == AutomaticMovementStatus::RECEIVE_DONE)
                        {
                            std::cout << "Writing ending status to shmem: " << +(gg->m_automatic_movement_status) << std::endl;
                            gg->m_automatic_execute_shmem_handler->shmemWrite(&gg->m_automatic_movement_status);
                            break;
                        }
                    }
                }
                else
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
            }
        }
        else
        {
            std::cout << "Cannot open shmem. Waiting 100 milliseconds." << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
}

void GuiGateway::handleArmDiagnostic(GuiGateway * gg)
{
    gg->m_diagnostic_shmem_handler = std::make_unique<ShmemHandler<DiagnosticData>>(
        odin::shmem_wrapper::DataTypes::DIAGNOSTIC_SHMEM_NAME, sizeof(DiagnosticData), false);
    gg->m_diagnostic_comm_handler = std::make_unique<InetCommHandler<DiagnosticData>>(
        sizeof(odin::diagnostic_handler::DiagnosticData), DIAGNOSTIC_SOCKET_PORT, ROBOTIC_GUI_IP);
    while (gg->m_run_process)
    {
        if (gg->m_diagnostic_shmem_handler->openShmem())
        {
            if (gg->m_diagnostic_shmem_handler->shmemRead(&(gg->m_remote_diagnostic)))
            {
                if (gg->m_remote_diagnostic != gg->m_previous_remote_diagnostic)
                {
                    gg->m_diagnostic_comm_handler->clientWrite(&(gg->m_remote_diagnostic));
                    gg->m_previous_remote_diagnostic = gg->m_remote_diagnostic;
                }
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
    }
}

void GuiGateway::handleArmControlSelection(GuiGateway * gg)
{
    gg->m_control_selection_shmem_handler = std::make_unique<ShmemHandler<OdinControlSelection>>(
        odin::shmem_wrapper::DataTypes::CONTROL_SELECT_SHMEM_NAME, sizeof(OdinControlSelection), true);
    gg->m_control_selection_comm_handler = std::make_unique<InetCommHandler<OdinControlSelection>>(
        sizeof(OdinControlSelection), CONTROL_SELECTION_PORT, ROBOTIC_GUI_IP);

    gg->m_control_selection_shmem_handler->shmemWrite(&(gg->m_control_selection));

    while (gg->m_run_process)
    {
        gg->m_control_selection_comm_handler->clientRead(&(gg->m_control_selection));
        if (gg->m_control_selection != gg->m_previous_control_selection)
        {
            gg->m_control_selection_shmem_handler->shmemWrite(&(gg->m_control_selection));
            gg->m_previous_control_selection = gg->m_control_selection;
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
}

void GuiGateway::handleArmAutomaticSteps(GuiGateway * gg)
{
    gg->m_automatic_execute_shmem_handler = std::make_unique<odin::shmem_wrapper::ShmemHandler<automatic_movement_status_t>>(
        odin::shmem_wrapper::DataTypes::AUTOMATIC_EXECUTE_SHMEM_NAME, sizeof(automatic_movement_status_t), true);
    gg->m_automatic_step_shmem_handler = std::make_unique<odin::shmem_wrapper::ShmemHandler<OdinServoStep>>(
        odin::shmem_wrapper::DataTypes::AUTOMATIC_STEP_SHMEM_NAME, sizeof(OdinServoStep), true);

    gg->m_automatic_execute_comm_handler = std::make_unique<InetCommHandler<automatic_movement_status_t>>(
        sizeof(automatic_movement_status_t), AUTOMATIC_EXECUTION_PORT, ROBOTIC_GUI_IP);
    gg->m_automatic_step_comm_handler = std::make_unique<InetCommHandler<OdinServoStep>>(
        sizeof(OdinServoStep), AUTOMATIC_SERVO_STEP_PORT, ROBOTIC_GUI_IP);

    OdinServoStep tmp_step;
    gg->m_automatic_step_shmem_handler->shmemWrite(&tmp_step);

    OdinServoStep servo_step;

    while (gg->m_run_process)
    {        
        bool data_start = false;

        while(!data_start)
        {
            if (gg->m_automatic_execute_comm_handler->clientRead(&gg->m_automatic_movement_status))
            {
                if (gg->m_automatic_movement_status == AutomaticMovementStatus::START_SENDING)
                {
                    std::cout << "Automatic process started." << std::endl;
                    data_start = true;
                    while(data_start)
                    {
                        if(gg->m_automatic_step_comm_handler->clientRead(&servo_step))
                        {
                            // Write servo instruction taken from server
                            gg->m_automatic_step_shmem_handler->shmemWrite(&servo_step);
                            
                            // Signal that servo data is ready to read
                            gg->m_automatic_execute_shmem_handler->shmemWrite(&gg->m_automatic_movement_status);
                            // TODO: Write error handling
                            while(true)
                            {
                                gg->m_automatic_execute_shmem_handler->shmemRead(&gg->m_automatic_movement_status);
                                if (gg->m_automatic_movement_status == AutomaticMovementStatus::RECEIVE_SUCCESS)
                                {
                                    data_start = false;
                                    break;
                                }
                            }
                        }
                        else
                        {
                            break;
                        }
                    }
                }
                else if (gg->m_automatic_movement_status == AutomaticMovementStatus::SEND_DONE)
                {
                    gg->m_automatic_execute_shmem_handler->shmemWrite(&gg->m_automatic_movement_status);
                    // TODO: Error handling
                    while(true)
                    {
                        gg->m_automatic_execute_shmem_handler->shmemRead(&gg->m_automatic_movement_status);
                        if (gg->m_automatic_movement_status == AutomaticMovementStatus::RECEIVE_DONE)
                        {
                            std::cout << "clientWrite RECEIVE_DONE" << std::endl;
                            gg->m_automatic_execute_comm_handler->clientWrite(&gg->m_automatic_movement_status);
                            break;
                        }
                    }
                }
            }
            else
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }
    }
}

void GuiGateway::signalCallbackHandler(int signum)
{
    InetCommHandler<DiagnosticData>::signalCallbackHandler(signum);
    InetCommHandler<OdinControlSelection>::signalCallbackHandler(signum);
    ShmemHandler<DiagnosticData>::signalCallbackHandler(signum);
    ShmemHandler<OdinControlSelection>::signalCallbackHandler(signum);
    std::cout << "GuiGateway received signal: " << signum << std::endl;
    m_run_process = false;
}

} // gui_gateway
} // odin