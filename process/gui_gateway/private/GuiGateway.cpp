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
    std::cout << "Starting scripted motion request thread." << std::endl;
    m_scripted_motion_request_thread = std::thread(handleGuiScriptedMotionRequest, this);
    m_diagnostic_thread.join();
    m_control_selection_thread.join();
    m_scripted_motion_request_thread.join();
}

void GuiGateway::runOnArm()
{
    std::cout << "Assigning threads." << std::endl;
    std::cout << "Starting ARM diagnostic thread." << std::endl;
    m_diagnostic_thread = std::thread(handleArmDiagnostic, this);
    std::cout << "Starting ARM control selection thread." << std::endl;
    m_control_selection_thread = std::thread(handleArmControlSelection, this);
    std::cout << "Starting automatic data thread." << std::endl;
    m_scripted_motion_request_thread = std::thread(handleArmScriptedMotionRequest, this);
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
        if(gg->m_diagnostic_comm_handler->serverRead(&(gg->m_remote_diagnostic)) == -1)
        {
            std::cout << "Client disconnected or read failed during GUI diagnostic handle." << std::endl;
            continue;
        }
        if (gg->m_remote_diagnostic != gg->m_previous_remote_diagnostic)
        {
            if(!gg->m_diagnostic_shmem_handler->shmemWrite(&(gg->m_remote_diagnostic)))
            {
                std::cout << "Error with writing to shmem during GUI diagnostic handle." << std::endl;
            }
            gg->m_previous_remote_diagnostic = gg->m_remote_diagnostic;
        }
        // TODO: change magic number to settings const for sleeps
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
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
                if(!gg->m_control_selection_comm_handler->serverWrite(&(gg->m_control_selection)))
                {
                    std::cout << "Error with writing data from server during GUI control mode selection handle." << std::endl;
                }
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}

void GuiGateway::handleGuiScriptedMotionRequest(GuiGateway * gg)
{
    gg->m_scripted_motion_request_shmem_status = std::make_unique<odin::shmem_wrapper::ShmemHandler<ScriptedMotionStepStatus>>(
        odin::shmem_wrapper::DataTypes::SCRIPTED_MOTION_REQUEST_STATUS_SHMEM_NAME, sizeof(ScriptedMotionStepStatus), false);
    gg->m_scripted_motion_reply_shmem_status = std::make_unique<odin::shmem_wrapper::ShmemHandler<ScriptedMotionStepStatus>>(
        odin::shmem_wrapper::DataTypes::SCRIPTED_MOTION_REPLY_STATUS_SHMEM_NAME, sizeof(ScriptedMotionStepStatus), false);
    gg->m_scripted_motion_step_shmem_handler = std::make_unique<odin::shmem_wrapper::ShmemHandler<OdinServoStep>>(
        odin::shmem_wrapper::DataTypes::SCRIPTED_MOTION_SERVO_STEP_SHMEM_NAME, sizeof(OdinServoStep), false);

    gg->m_scripted_motion_request_inet_handler = std::make_unique<InetCommHandler<ScriptedMotionStepStatus>>(
        sizeof(ScriptedMotionStepStatus), SCRIPTED_MOTION_REQUEST_PORT);
    gg->m_scripted_motion_step_inet_handler = std::make_unique<InetCommHandler<OdinServoStep>>(
        sizeof(OdinServoStep), SCRIPTED_MOTION_SERVO_DATA_PORT);

    std::uint64_t current_step_monitor = 0;
    OdinServoStep servo_step;

    ScriptedMotionStepStatus current_step_local_request_status;
    ScriptedMotionStepStatus current_step_local_reply_status;
    ScriptedMotionStepStatus current_step_remote_request_status;
    ScriptedMotionStepStatus current_step_remote_reply_status;

    bool handle_motion_request = false;
    std::uint8_t connection_retries = 0;
    
    while (gg->m_run_process)
    {
        // Check client connection
        if (!gg->m_scripted_motion_request_inet_handler->serverWrite(&current_step_remote_request_status))
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            ++connection_retries;
            if (connection_retries == 10)
            {
                std::cout << "Error while checking client connection. Waiting 10 seconds before retry." << std::endl;
                std::this_thread::sleep_for(std::chrono::seconds(1));
                connection_retries = 0;
            }
        }
        current_step_monitor = 0;
        if (gg->m_scripted_motion_request_shmem_status->openShmem() && gg->m_scripted_motion_reply_shmem_status->openShmem() && gg->m_scripted_motion_step_shmem_handler->openShmem())
        {
            if (gg->m_scripted_motion_request_shmem_status->shmemRead(&current_step_local_request_status))
            {
                if (current_step_local_request_status.step_status == static_cast<scripted_motion_status_t>(ScriptedMotionRequestStatus::START_REQUEST))
                {
                    current_step_remote_request_status.step_status = static_cast<scripted_motion_status_t>(ScriptedMotionRequestStatus::START_REQUEST);
                    current_step_remote_reply_status.step_status = static_cast<scripted_motion_status_t>(ScriptedMotionReplyStatus::WAITING);

                    current_step_monitor = current_step_local_request_status.step_num;
                    handle_motion_request = true;

                    if (!gg->m_scripted_motion_request_inet_handler->serverWrite(&current_step_remote_request_status))
                    {
                        std::cout << "Error while writing request status to client. Stopping the request." << std::endl;
                        handle_motion_request = false;
                        current_step_local_reply_status.step_status = static_cast<scripted_motion_status_t>(ScriptedMotionReplyStatus::DISCONNECTED);
                        if (!gg->m_scripted_motion_reply_shmem_status->shmemWrite(&current_step_local_reply_status))
                        {
                            std::cout << "Error while writing scripted motion step status to GUI." << std::endl;
                        }
                    }
                    std::cout << "Wrote to clinet about request." << std::endl;
                }
            }
            else
            {
                std::cout << "Error while reading scripted motion request status SHMEM" << std::endl;
                handle_motion_request = false;
            }
            while (handle_motion_request && gg->m_run_process)
            {
                if (!gg->m_scripted_motion_request_shmem_status->shmemRead(&current_step_local_request_status))
                {
                    std::cout << "Error while reading scripted motion step request status from GUI." << std::endl;
                    handle_motion_request = false;
                    break;
                }
                if ((current_step_local_request_status.step_status == static_cast<scripted_motion_status_t>(ScriptedMotionRequestStatus::STOP_REQUESTED) ||
                    current_step_local_request_status.step_status == static_cast<scripted_motion_status_t>(ScriptedMotionRequestStatus::REQUEST_COMPLETE)) &&
                    handle_motion_request)
                {
                    current_step_remote_request_status.step_status = static_cast<scripted_motion_status_t>(ScriptedMotionRequestStatus::REQUEST_COMPLETE);
                    if (!gg->m_scripted_motion_request_inet_handler->serverWrite(&current_step_remote_request_status))
                    {
                        std::cout << "Error while writing request status to client. Stopping the request." << std::endl;
                        handle_motion_request = false;
                        current_step_local_reply_status.step_status = static_cast<scripted_motion_status_t>(ScriptedMotionReplyStatus::DISCONNECTED);
                        if (!gg->m_scripted_motion_reply_shmem_status->shmemWrite(&current_step_local_reply_status))
                        {
                            std::cout << "Error while writing scripted motion step status to GUI." << std::endl;
                        }
                    }
                    current_step_remote_request_status.step_num = 0;
                    current_step_remote_request_status.step_status = static_cast<scripted_motion_status_t>(ScriptedMotionRequestStatus::NONE);

                    std::cout << "Scripted motion request done." << std::endl;
                    handle_motion_request = false;
                    break;
                }
                if (current_step_local_request_status.step_status != static_cast<scripted_motion_status_t>(ScriptedMotionRequestStatus::STOP_REQUESTED) &&
                    current_step_local_request_status.step_status != static_cast<scripted_motion_status_t>(ScriptedMotionRequestStatus::REQUEST_COMPLETE) &&
                    current_step_local_reply_status.step_status != static_cast<scripted_motion_status_t>(ScriptedMotionReplyStatus::WAITING) &&
                    handle_motion_request)
                {
                    current_step_local_reply_status.step_num = current_step_local_request_status.step_num;
                    current_step_local_reply_status.step_status = static_cast<scripted_motion_status_t>(ScriptedMotionReplyStatus::WAITING);
                    if (!gg->m_scripted_motion_reply_shmem_status->shmemWrite(&current_step_local_reply_status))
                    {
                        std::cout << "Error while writing scripted motion step status to GUI." << std::endl;
                        handle_motion_request = false;
                        break;
                    }
                }
                if (current_step_local_request_status.step_status == static_cast<scripted_motion_status_t>(ScriptedMotionRequestStatus::EXECUTE_ON_ARM) &&
                    current_step_local_reply_status.step_status != static_cast<scripted_motion_status_t>(ScriptedMotionReplyStatus::IN_PROGRESS) &&
                    current_step_monitor == current_step_local_request_status.step_num &&
                    handle_motion_request) 
                {
                    if (!gg->m_scripted_motion_step_shmem_handler->shmemRead(&servo_step))
                    {
                        std::cout << "Error while reading scripted motion servo step data." << std::endl;
                        handle_motion_request = false;
                        break;
                    }
                    current_step_local_reply_status.step_status = static_cast<scripted_motion_status_t>(ScriptedMotionReplyStatus::IN_PROGRESS);
                    std::cout << "Writing servo data to ARM:" << std::endl
                            << +servo_step.step_num << std::endl
                            << +servo_step.servo_num << std::endl
                            << +servo_step.position << std::endl
                            << +servo_step.speed << std::endl
                            << +servo_step.delay << std::endl
                            << "---" << std::endl;
                    if (!gg->m_scripted_motion_step_inet_handler->serverWrite(&servo_step))
                    {
                        std::cout << "Couldn't write servo step to client. Stopping the request." << std::endl;
                        handle_motion_request = false;
                        break;
                    }
                    ++current_step_monitor;
                }
                while (current_step_local_request_status.step_status == static_cast<scripted_motion_status_t>(ScriptedMotionRequestStatus::EXECUTE_ON_ARM) &&
                       current_step_remote_reply_status.step_status != static_cast<scripted_motion_status_t>(ScriptedMotionReplyStatus::COMPLETED) &&
                       handle_motion_request &&
                       gg->m_run_process)
                {
                    if (gg->m_scripted_motion_request_inet_handler->serverRead(&current_step_remote_reply_status) < 0)
                    {
                        std::cout << "Error while reading motion request status from remote. Stopping the request," << std::endl;
                        current_step_remote_reply_status.step_status = static_cast<scripted_motion_status_t>(ScriptedMotionReplyStatus::DISCONNECTED);
                        handle_motion_request = false;
                        break;
                    }
                }
                if (current_step_local_request_status.step_status == static_cast<scripted_motion_status_t>(ScriptedMotionRequestStatus::EXECUTE_ON_ARM) &&
                    current_step_local_reply_status.step_status == static_cast<scripted_motion_status_t>(ScriptedMotionReplyStatus::IN_PROGRESS) &&
                    current_step_remote_reply_status.step_status == static_cast<scripted_motion_status_t>(ScriptedMotionReplyStatus::COMPLETED) &&
                    handle_motion_request)
                {
                    if (current_step_remote_reply_status.step_num != current_step_local_reply_status.step_num)
                    {
                        std::cout << "Wrong step status received from ARM. Stopping request." << std::endl;
                        std::cout << +current_step_remote_reply_status.step_num << " " << +current_step_local_reply_status.step_num << std::endl;
                        handle_motion_request = false;
                        break;
                    }
                    current_step_local_reply_status.step_num = current_step_local_request_status.step_num;
                    current_step_local_reply_status.step_status = static_cast<scripted_motion_status_t>(ScriptedMotionReplyStatus::COMPLETED);
                    if (!gg->m_scripted_motion_reply_shmem_status->shmemWrite(&current_step_local_reply_status))
                    {
                        std::cout << "Error while writing scripted motion step completed status to GUI." << std::endl;
                        handle_motion_request = false;
                        break;
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
                if (current_step_remote_reply_status.step_status == static_cast<scripted_motion_status_t>(ScriptedMotionReplyStatus::ERROR))
                {
                    std::cout << "Error status received from client. Stopping request." << std::endl;
                    handle_motion_request = false;
                    break;
                }
                current_step_remote_reply_status.step_status = static_cast<scripted_motion_status_t>(ScriptedMotionReplyStatus::WAITING);
            }
        }
        else
        {
            std::cout << "Cannot open scripted motion SHMEM..." << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

void GuiGateway::handleArmDiagnostic(GuiGateway * gg)
{
    gg->m_diagnostic_shmem_handler = std::make_unique<ShmemHandler<DiagnosticData>>(
        odin::shmem_wrapper::DataTypes::DIAGNOSTIC_SHMEM_NAME, sizeof(DiagnosticData), false);
    gg->m_diagnostic_comm_handler = std::make_unique<InetCommHandler<DiagnosticData>>(
        sizeof(odin::diagnostic_handler::DiagnosticData), DIAGNOSTIC_SOCKET_PORT, ROBOTIC_GUI_IP);
    std::uint8_t failed_send_count = 0;
    while (gg->m_run_process)
    {
        if (gg->m_diagnostic_shmem_handler->openShmem())
        {
            if (gg->m_diagnostic_shmem_handler->shmemRead(&(gg->m_remote_diagnostic)))
            {
                if (gg->m_remote_diagnostic != gg->m_previous_remote_diagnostic)
                {
                    if (!gg->m_diagnostic_comm_handler->clientWrite(&(gg->m_remote_diagnostic)))
                    {
                        ++failed_send_count;
                        std::cout << "Warning: Failed to send diagnostic data to GUI [attempt " << +failed_send_count << "/3]" << std::endl;
                        if (failed_send_count >= 3)
                        {
                            std::cout << "Too many failures, backing off for 1 second..." << std::endl;
                            std::this_thread::sleep_for(std::chrono::seconds(1));
                            failed_send_count = 0;
                        }
                        else
                        {
                            std::this_thread::sleep_for(std::chrono::milliseconds(10));
                        }
                        continue;
                    }
                    gg->m_previous_remote_diagnostic = gg->m_remote_diagnostic;
                }
                failed_send_count = 0;
            }
            else 
            {
                std::cout << "Arm diagnostic handle can't read SHMEM." << std::endl;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
}

void GuiGateway::handleArmControlSelection(GuiGateway * gg)
{
    gg->m_control_selection_shmem_handler = std::make_unique<ShmemHandler<OdinControlSelection>>(
        odin::shmem_wrapper::DataTypes::CONTROL_SELECT_SHMEM_NAME, sizeof(OdinControlSelection), true);
    gg->m_control_selection_comm_handler = std::make_unique<InetCommHandler<OdinControlSelection>>(
        sizeof(OdinControlSelection), CONTROL_SELECTION_PORT, ROBOTIC_GUI_IP);

    if(!gg->m_control_selection_shmem_handler->shmemWrite(&(gg->m_control_selection)))
    {
        std::cout << "Error during initial shmem write of Arm Control Selection mode." << std::endl;
    }
    std::uint8_t failed_reads = 0;
    while (gg->m_run_process)
    {
        if (gg->m_control_selection_comm_handler->clientRead(&(gg->m_control_selection)) == -1)
        {
            ++failed_reads;
            if (failed_reads >= 3) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
                failed_reads = 0;
            }
            std::cout << "Error with Arm Control Selection mode read from server during Arm Control Selection handle." << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(60));
            continue;
        }
        failed_reads = 0;
        if (gg->m_control_selection != gg->m_previous_control_selection)
        {
            if(!gg->m_control_selection_shmem_handler->shmemWrite(&(gg->m_control_selection)))
            {
                std::cout << "Error with writing Arm Control Selection mode to shmem." << std::endl;
                continue;
            }
            gg->m_previous_control_selection = gg->m_control_selection;
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
    }
}

void GuiGateway::handleArmScriptedMotionRequest(GuiGateway * gg)
{
    gg->m_scripted_motion_request_shmem_status = std::make_unique<odin::shmem_wrapper::ShmemHandler<ScriptedMotionStepStatus>>(
        odin::shmem_wrapper::DataTypes::SCRIPTED_MOTION_REQUEST_STATUS_SHMEM_NAME, sizeof(ScriptedMotionStepStatus), true);
    gg->m_scripted_motion_step_shmem_handler = std::make_unique<odin::shmem_wrapper::ShmemHandler<OdinServoStep>>(
        odin::shmem_wrapper::DataTypes::SCRIPTED_MOTION_SERVO_STEP_SHMEM_NAME, sizeof(OdinServoStep), true);
    gg->m_scripted_motion_reply_shmem_status = std::make_unique<odin::shmem_wrapper::ShmemHandler<ScriptedMotionStepStatus>>(
        odin::shmem_wrapper::DataTypes::SCRIPTED_MOTION_REPLY_STATUS_SHMEM_NAME, sizeof(ScriptedMotionStepStatus), true);

    gg->m_scripted_motion_request_inet_handler = std::make_unique<InetCommHandler<ScriptedMotionStepStatus>>(
        sizeof(ScriptedMotionStepStatus), SCRIPTED_MOTION_REQUEST_PORT, ROBOTIC_GUI_IP);
    gg->m_scripted_motion_step_inet_handler = std::make_unique<InetCommHandler<OdinServoStep>>(
        sizeof(OdinServoStep), SCRIPTED_MOTION_SERVO_DATA_PORT, ROBOTIC_GUI_IP);

    bool handle_motion_request = false;
    std::uint8_t connection_retries = 0;

    std::uint64_t current_step_monitor = 0;
    OdinServoStep servo_step;

    ScriptedMotionStepStatus current_step_local_request_status;
    ScriptedMotionStepStatus current_step_local_reply_status;
    ScriptedMotionStepStatus current_step_remote_request_status;
    ScriptedMotionStepStatus current_step_remote_reply_status;

    while (gg->m_run_process)
    {       
        // Check client connection and read request status
        if (gg->m_scripted_motion_request_inet_handler->clientRead(&current_step_remote_request_status) < 0)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            ++connection_retries;
            if (connection_retries == 10)
            {
                std::cout << "Error while checking server connection. Waiting 10 seconds before retry." << std::endl;
                std::this_thread::sleep_for(std::chrono::seconds(1));
                connection_retries = 0;
            }
        }
        current_step_monitor = 0;
        current_step_remote_reply_status.step_num = 0;
        if (current_step_remote_request_status.step_status == static_cast<scripted_motion_status_t>(ScriptedMotionRequestStatus::START_REQUEST))
        {
            std::cout << "START REQUEST" << std::endl;
            handle_motion_request = true;
            if (gg->m_scripted_motion_request_shmem_status->openShmem() && gg->m_scripted_motion_reply_shmem_status->openShmem() && gg->m_scripted_motion_step_shmem_handler->openShmem())
            {
                while (handle_motion_request && gg->m_run_process)
                {
                    if (gg->m_scripted_motion_step_inet_handler->clientRead(&servo_step))
                    {
                        std::cout << "START REQUEST 1" << std::endl;
                        if (current_step_remote_reply_status.step_num != current_step_monitor)
                        {
                            std::cout << "Error while processing remote motion request: step number mismatch. Stopping the request;" << std::endl;
                            std::cout << +current_step_monitor << " " << +current_step_remote_reply_status.step_num << std::endl;
                            current_step_remote_reply_status.step_status = static_cast<scripted_motion_status_t>(ScriptedMotionReplyStatus::ERROR);
                            gg->m_scripted_motion_request_inet_handler->clientWrite(&current_step_remote_reply_status);
                        }
                        // TODO: write logic to send data to arm controller
                        std::cout << "Writing servo data to ARM:" << std::endl
                            << +servo_step.step_num << std::endl
                            << +servo_step.servo_num << std::endl
                            << +servo_step.position << std::endl
                            << +servo_step.speed << std::endl
                            << +servo_step.delay << std::endl
                            << "---" << std::endl;
                        //
                        current_step_remote_reply_status.step_status = static_cast<scripted_motion_status_t>(ScriptedMotionReplyStatus::COMPLETED);
                        std::cout << +current_step_monitor << " " << +current_step_remote_reply_status.step_num << std::endl;
                        gg->m_scripted_motion_request_inet_handler->clientWrite(&current_step_remote_reply_status);
                        ++current_step_monitor;
                        current_step_remote_reply_status.step_num = current_step_monitor;
                    }
                    if (gg->m_scripted_motion_request_inet_handler->clientRead(&current_step_remote_request_status) < 0)
                    {
                        std::cout << "START REQUEST 2" << std::endl;
                        //HANDLE CONNECTION ERROR
                    }
                    if (current_step_remote_request_status.step_status == static_cast<scripted_motion_status_t>(ScriptedMotionRequestStatus::REQUEST_COMPLETE))
                    {
                        std::cout << "START REQUEST 3" << std::endl;
                        handle_motion_request = false;
                    }
                }
            }
            else
            {
                std::cout << "Cannot open scripted motion SHMEM..." << std::endl;
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    //     bool data_start = false;

    //     while (!data_start && gg->m_run_process)
    //     {
    //         if (gg->m_scripted_motion_request_inet_handler->clientRead(&gg->m_scripted_motion_request_status))
    //         {
    //             if (gg->m_scripted_motion_request_status == ScriptedMotionReplyStatus::START_SENDING)
    //             {
    //                 std::cout << "Automatic Movement process started." << std::endl;
    //                 data_start = true;

    //                 while (data_start && gg->m_run_process)
    //                 {
    //                     if (gg->m_scripted_motion_step_inet_handler->clientRead(&servo_step))
    //                     {
    //                         // Write servo instruction taken from server
    //                         if (!gg->m_scripted_motion_step_shmem_handler->shmemWrite(&servo_step))
    //                         {
    //                             std::cout << "Failed to write servo step to SHMEM." << std::endl;
    //                             continue;
    //                         }
                            
    //                         if (!gg->m_scripted_motion_request_shmem_status->shmemWrite(&gg->m_scripted_motion_request_status))
    //                         {
    //                             std::cout << "Failed to write Automatic Movement Status to SHMEM." << std::endl;
    //                             continue;
    //                         }
                            
    //                         std::uint16_t wait_timeout = 0;
    //                         while (wait_timeout < 5000 && gg->m_run_process)
    //                         {
    //                             if (!gg->m_scripted_motion_request_shmem_status->shmemRead(&gg->m_scripted_motion_request_status))
    //                             {
    //                                 std::cout << "Failed to read Automatic Movement Status from SHMEM." << std::endl;
    //                                 break;
    //                             }
    //                             if (gg->m_scripted_motion_request_status == ScriptedMotionReplyStatus::RECEIVE_SUCCESS)
    //                             {
    //                                 data_start = false;
    //                                 break;
    //                             }
    //                             std::this_thread::sleep_for(std::chrono::milliseconds(10));
    //                             wait_timeout += 10;
    //                         }
    //                         if (wait_timeout >= 5000)
    //                         {
    //                             std::cout << "[TIMEOUT] Waiting for RECEIVE_SUCCESS failed." << std::endl;
    //                             break;
    //                         }
    //                     }
    //                     else
    //                     {
    //                         std::cout << "Failed to read servo step from TCP." << std::endl;
    //                         break;
    //                     }
    //                 }
    //             }
    //             else if (gg->m_scripted_motion_request_status == ScriptedMotionReplyStatus::SEND_DONE)
    //             {
    //                 if (!gg->m_scripted_motion_request_shmem_status->shmemWrite(&gg->m_scripted_motion_request_status))
    //                 {
    //                     std::cout << "Failed to write SEND_DONE to SHMEM." << std::endl;
    //                     continue;
    //                 }
    //                 std::uint16_t wait_timeout = 0;
    //                 while(wait_timeout < 5000 && gg->m_run_process)
    //                 {
    //                     if (!gg->m_scripted_motion_request_shmem_status->shmemRead(&gg->m_scripted_motion_request_status))
    //                     {
    //                         std::cout << "Failed to read from SHMEM waiting for RECEIVE_DONE." << std::endl;
    //                         break;
    //                     }
    //                     if (gg->m_scripted_motion_request_status == ScriptedMotionReplyStatus::RECEIVE_DONE)
    //                     {
    //                         if (!gg->m_scripted_motion_request_inet_handler->clientWrite(&gg->m_scripted_motion_request_status))
    //                         {
    //                             std::cout << "Failed to send RECEIVE_DONE to GUI." << std::endl;
    //                         }
    //                         break;
    //                     }
    //                     std::this_thread::sleep_for(std::chrono::milliseconds(10));
    //                     wait_timeout += 10;
    //                 }
    //                 if (wait_timeout >= 5000)
    //                 {
    //                     std::cout << "[TIMEOUT] Waiting for RECEIVE_DONE failed." << std::endl;
    //                 }
    //             }
    //         }
    //         else
    //         {
    //             std::this_thread::sleep_for(std::chrono::milliseconds(1));
    //         }
    //     }
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