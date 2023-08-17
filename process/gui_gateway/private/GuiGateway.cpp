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
    gg->m_automatic_execute_shmem_handler = std::make_unique<odin::shmem_wrapper::ShmemHandler<OdinAutomaticExecuteData>>(
        odin::shmem_wrapper::DataTypes::AUTOMATIC_EXECUTE_SHMEM_NAME, sizeof(OdinAutomaticExecuteData), false);
    gg->m_automatic_step_shmem_handler = std::make_unique<odin::shmem_wrapper::ShmemHandler<OdinServoStep>>(
        odin::shmem_wrapper::DataTypes::AUTOMATIC_STEP_SHMEM_NAME, sizeof(OdinServoStep), false);

    gg->m_automatic_execute_comm_handler = std::make_unique<InetCommHandler<OdinAutomaticExecuteData>>(
        sizeof(OdinAutomaticExecuteData), AUTOMATIC_EXECUTION_PORT);
    gg->m_automatic_step_comm_handler = std::make_unique<InetCommHandler<OdinServoStep>>(
        sizeof(OdinServoStep), AUTOMATIC_SERVO_STEP_PORT);
    
    while (gg->m_run_process)
    {
        OdinAutomaticExecuteData automatic_execute_data;
        OdinServoStep servo_step;
        bool data_start = false;
        while (!data_start)
        {
            if (gg->m_automatic_execute_shmem_handler->openShmem())
            {
                if (gg->m_automatic_execute_shmem_handler->shmemRead(&automatic_execute_data, true))
                {
                    if (automatic_execute_data.data_collection_status == 1)
                    {
                        data_start = true;
                        std::cout << "Automatic process started." << std::endl;
                        gg->m_automatic_execute_comm_handler->serverWrite(&automatic_execute_data);
                    }
                    else
                    {
                        std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    }
                }
            }
            else
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        int step_num = 0;
        while (data_start)
        {
            if (gg->m_automatic_step_shmem_handler->shmemRead(&servo_step, true))
            {
                if (servo_step.step_num == step_num)
                {
                    std::cout << "Reading step." << std::endl;
                    ++step_num;
                    std::cout << servo_step.step_num << std::endl;
                    std::cout << +servo_step.servo_num << std::endl;
                    std::cout << servo_step.position << std::endl;
                    std::cout << +servo_step.speed << std::endl;
                    std::cout << servo_step.delay << std::endl;
                    gg->m_automatic_step_comm_handler->serverWrite(&servo_step);
                }
            }
            else
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            if (gg->m_automatic_execute_shmem_handler->shmemRead(&automatic_execute_data, true))
            {
                if (automatic_execute_data.data_collection_status == 2)
                {
                    data_start = false;
                    std::cout << "Data collection finished." << std::endl;
                    gg->m_automatic_execute_comm_handler->serverWrite(&automatic_execute_data);
                }
            }
            else
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
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
    gg->m_automatic_execute_shmem_handler = std::make_unique<odin::shmem_wrapper::ShmemHandler<OdinAutomaticExecuteData>>(
        odin::shmem_wrapper::DataTypes::AUTOMATIC_EXECUTE_SHMEM_NAME, sizeof(OdinAutomaticExecuteData), true);
    gg->m_automatic_step_shmem_handler = std::make_unique<odin::shmem_wrapper::ShmemHandler<OdinServoStep>>(
        odin::shmem_wrapper::DataTypes::AUTOMATIC_STEP_SHMEM_NAME, sizeof(OdinServoStep), true);

    gg->m_automatic_execute_comm_handler = std::make_unique<InetCommHandler<OdinAutomaticExecuteData>>(
        sizeof(OdinAutomaticExecuteData), AUTOMATIC_EXECUTION_PORT, ROBOTIC_GUI_IP);
    gg->m_automatic_step_comm_handler = std::make_unique<InetCommHandler<OdinServoStep>>(
        sizeof(OdinServoStep), AUTOMATIC_SERVO_STEP_PORT, ROBOTIC_GUI_IP);

    OdinServoStep tmp_step;
    gg->m_automatic_step_shmem_handler->shmemWrite(&tmp_step);

    while (gg->m_run_process)
    {
        OdinAutomaticExecuteData automatic_execute_data;
        OdinServoStep servo_step;
        bool data_start = false;
        // TODO: write blocking reading and writing shmem wrapper methods
        while(!data_start)
        {
            gg->m_automatic_execute_comm_handler->clientRead(&automatic_execute_data);
            if (automatic_execute_data.data_collection_status == 1)
            {
                std::cout << "Automatic process started." << std::endl;
                data_start = true;
                gg->m_automatic_execute_shmem_handler->shmemWrite(&automatic_execute_data, true);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        
        bool finish_reading = false;
        int step_num = 0;
        while (!finish_reading)
        {
            while(gg->m_automatic_step_comm_handler->clientRead(&servo_step))
            {
                std::cout << "Reading servo_step" << std::endl;
                std::cout << servo_step.step_num << std::endl;
                std::cout << +servo_step.servo_num << std::endl;
                std::cout << servo_step.position << std::endl;
                std::cout << +servo_step.speed << std::endl;
                std::cout << servo_step.delay << std::endl;

                gg->m_automatic_step_shmem_handler->shmemWrite(&servo_step, true);
            }
            gg->m_automatic_execute_comm_handler->clientRead(&automatic_execute_data);
            if (automatic_execute_data.data_collection_status == 2)
            {
                std::cout << "Reading from server finished." << std::endl;
                finish_reading = true;
                gg->m_automatic_execute_shmem_handler->shmemWrite(&automatic_execute_data, true);        
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