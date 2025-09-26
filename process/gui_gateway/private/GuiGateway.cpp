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
    m_scripted_motion_request_thread.join();
}

void GuiGateway::handleGuiDiagnostic(GuiGateway * gg)
{
    gg->m_diagnostic_shmem_handler = std::make_unique<ShmemHandler<DiagnosticData>>(
        odin::shmem_wrapper::DataTypes::DIAGNOSTIC_FROM_REMOTE_SHMEM_NAME, sizeof(DiagnosticData), true);
    gg->m_diagnostic_comm_handler = std::make_unique<TcpHandler<DiagnosticData>>(
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
    gg->m_control_selection_comm_handler = std::make_unique<TcpHandler<OdinControlSelection>>(
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
    gg->m_scripted_motion_request_shmem_status = std::make_unique<odin::shmem_wrapper::ShmemHandler<ScriptedMotionStepData>>(
        odin::shmem_wrapper::DataTypes::SCRIPTED_MOTION_REQUEST_STATUS_SHMEM_NAME, sizeof(ScriptedMotionStepData), false);
    gg->m_scripted_motion_reply_shmem_status = std::make_unique<odin::shmem_wrapper::ShmemHandler<ScriptedMotionStepData>>(
        odin::shmem_wrapper::DataTypes::SCRIPTED_MOTION_REPLY_STATUS_SHMEM_NAME, sizeof(ScriptedMotionStepData), false);
    gg->m_scripted_motion_step_shmem_handler = std::make_unique<odin::shmem_wrapper::ShmemHandler<OdinServoStep>>(
        odin::shmem_wrapper::DataTypes::SCRIPTED_MOTION_SERVO_STEP_SHMEM_NAME, sizeof(OdinServoStep), false);

    gg->m_scripted_motion_request_inet_handler = std::make_unique<TcpHandler<ScriptedMotionStepData>>(
        sizeof(ScriptedMotionStepData), SCRIPTED_MOTION_REQUEST_PORT);
    gg->m_scripted_motion_step_inet_handler = std::make_unique<TcpHandler<OdinServoStep>>(
        sizeof(OdinServoStep), SCRIPTED_MOTION_SERVO_DATA_PORT);

    std::uint64_t current_step_index = 0;
    OdinServoStep servo_step;

    auto req_status = ScriptedMotionStatus::IDLE;
    auto rep_status = ScriptedMotionStatus::IDLE;

    auto gg_req_status = ScriptedMotionStatus::IDLE;
    auto gg_rep_status = ScriptedMotionStatus::IDLE;

    ScriptedMotionStepData req_data{current_step_index, req_status};
    ScriptedMotionStepData rep_data{current_step_index, rep_status};

    ScriptedMotionStepData gg_req_data{current_step_index, gg_req_status};
    ScriptedMotionStepData gg_rep_data{current_step_index, gg_rep_status};

    std::uint8_t connection_retries = 0;

    enum class Phase { Idle, StartReq, HandleRequest, WaitArmComplete, StopReq, EndReq };
    Phase phase = Phase::Idle;

    bool log_phase = true;

    while (gg->m_run_process)
    {
        if (gg->m_scripted_motion_request_shmem_status->openShmem() && gg->m_scripted_motion_reply_shmem_status->openShmem() && gg->m_scripted_motion_step_shmem_handler->openShmem())
        {
            switch (phase)
            {
                case Phase::Idle:
                {
                    if (log_phase)
                    {
                        std::cout << "IDLE" << std::endl;
                        log_phase = false;
                    } 
                    if (!gg->m_scripted_motion_request_shmem_status->shmemRead(&req_data))
                    {
                        std::cout << "[GUI GATEWAY] Error: Couldn't read automated motion request SHMEM." << std::endl;
                        break;
                    }
                    if (req_data.step_status == ScriptedMotionStatus::START_REQUEST)
                    {
                        if (req_data.step_num == 0 && current_step_index != 0)
                            current_step_index = 0;
                        if (req_data.step_num == current_step_index)
                        {
                            std::cout << "Start request" << std::endl;
                            gg_req_data.step_num = current_step_index;
                            gg_req_data.step_status = ScriptedMotionStatus::START_REQUEST;
                            gg->m_scripted_motion_request_inet_handler->serverWrite(&gg_req_data);
                            phase = Phase::HandleRequest;
                            log_phase = true;
                        }
                    }
                    else if (req_data.step_status != ScriptedMotionStatus::START_REQUEST)
                    {
                        current_step_index = 0;
                        rep_data.step_num = current_step_index;
                        rep_data.step_status = ScriptedMotionStatus::IDLE;
                        gg->m_scripted_motion_request_inet_handler->serverWrite(&rep_data);
                        if (!gg->m_scripted_motion_reply_shmem_status->shmemWrite(&rep_data))
                        {
                            std::cout << "[GUI GATEWAY] Error: Couldn't write automated motion reply SHMEM." << std::endl;
                            break;
                        }
                    }
                    break;
                }
                case Phase::HandleRequest:
                {
                    if (log_phase)
                    {
                        std::cout << "HANDLE_REQUEST" << std::endl;
                        log_phase = false;
                    } 
                    std::cout << "Handling request data. Step num: " << current_step_index << std::endl;
                    if (gg->m_scripted_motion_request_shmem_status->shmemRead(&req_data) &&
                        req_data.step_status != ScriptedMotionStatus::START_REQUEST)
                    { phase = Phase::Idle; log_phase = true; break; }
                    if (!gg->m_scripted_motion_step_shmem_handler->shmemRead(&servo_step))
                    {
                        std::cout << "[GUI GATEWAY] Error: Couldn't read automated motion request SHMEM." << std::endl;
                        phase = Phase::Idle;
                        log_phase = true;
                        break;
                    }
                    gg->m_scripted_motion_step_inet_handler->serverWrite(&servo_step);
                    phase = Phase::WaitArmComplete;
                    break;
                }
                case Phase::WaitArmComplete:
                {
                    if (log_phase)
                    {
                        std::cout << "WAIT_ARM_COMPLETE" << std::endl;
                        log_phase = false;
                    } 
                    // Check if request was cancelled
                    if (!gg->m_scripted_motion_request_shmem_status->shmemRead(&req_data))
                    {
                        std::cout << "[GUI GATEWAY] Error: Couldn't read automated motion request SHMEM." << std::endl;
                        break;
                    }
                    if (req_data.step_status != ScriptedMotionStatus::START_REQUEST)
                    {
                        current_step_index = 0;
                        phase = Phase::Idle;
                        log_phase = true;
                    }
                    int rc = gg->m_scripted_motion_request_inet_handler->serverRead(&gg_rep_data);
                    if (rc < 0) { std::this_thread::sleep_for(std::chrono::milliseconds(100)); break; }
                    if (rc == 0) { break; }
                    if (gg_rep_data.step_num == current_step_index &&
                        gg_rep_data.step_status == ScriptedMotionStatus::REQUEST_COMPLETED)
                    {
                        rep_data.step_num = current_step_index;
                        rep_data.step_status = ScriptedMotionStatus::REQUEST_COMPLETED;
                        if (!gg->m_scripted_motion_reply_shmem_status->shmemWrite(&rep_data))
                        {
                            std::cout << "[GUI GATEWAY] Error: Couldn't write automated motion reply SHMEM." << std::endl;
                            phase = Phase::Idle; log_phase = true; break;
                        }
                        ++current_step_index;
                        phase = Phase::Idle; log_phase = true;
                    }
                    break;
                }
                case Phase::StopReq:
                {
                    break;
                }
                default:
                    break;
            }
        }
        else
        {
            std::cout << "Cannot open scripted motion SHMEM..." << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
}

void GuiGateway::handleArmDiagnostic(GuiGateway * gg)
{
    gg->m_diagnostic_shmem_handler = std::make_unique<ShmemHandler<DiagnosticData>>(
        odin::shmem_wrapper::DataTypes::DIAGNOSTIC_SHMEM_NAME, sizeof(DiagnosticData), false);
    gg->m_diagnostic_comm_handler = std::make_unique<TcpHandler<DiagnosticData>>(
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
    gg->m_control_selection_comm_handler = std::make_unique<TcpHandler<OdinControlSelection>>(
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
    gg->m_scripted_motion_request_shmem_status = std::make_unique<odin::shmem_wrapper::ShmemHandler<ScriptedMotionStepData>>(
        odin::shmem_wrapper::DataTypes::SCRIPTED_MOTION_REQUEST_STATUS_SHMEM_NAME, sizeof(ScriptedMotionStepData), true);
    gg->m_scripted_motion_reply_shmem_status = std::make_unique<odin::shmem_wrapper::ShmemHandler<ScriptedMotionStepData>>(
        odin::shmem_wrapper::DataTypes::SCRIPTED_MOTION_REPLY_STATUS_SHMEM_NAME, sizeof(ScriptedMotionStepData), true);
    gg->m_scripted_motion_step_shmem_handler = std::make_unique<odin::shmem_wrapper::ShmemHandler<OdinServoStep>>(
        odin::shmem_wrapper::DataTypes::SCRIPTED_MOTION_SERVO_STEP_SHMEM_NAME, sizeof(OdinServoStep), true);

    gg->m_scripted_motion_request_inet_handler = std::make_unique<TcpHandler<ScriptedMotionStepData>>(
        sizeof(ScriptedMotionStepData), SCRIPTED_MOTION_REQUEST_PORT, ROBOTIC_GUI_IP);
    gg->m_scripted_motion_step_inet_handler = std::make_unique<TcpHandler<OdinServoStep>>(
        sizeof(OdinServoStep), SCRIPTED_MOTION_SERVO_DATA_PORT, ROBOTIC_GUI_IP);

    std::uint64_t current_step_index = 0;
    OdinServoStep servo_step;

    auto req_status = ScriptedMotionStatus::IDLE;
    auto rep_status = ScriptedMotionStatus::IDLE;

    auto gg_req_status = ScriptedMotionStatus::IDLE;
    auto gg_rep_status = ScriptedMotionStatus::IDLE;

    ScriptedMotionStepData req_data{current_step_index, req_status};
    ScriptedMotionStepData rep_data{current_step_index, rep_status};

    ScriptedMotionStepData gg_req_data{current_step_index, gg_req_status};
    ScriptedMotionStepData gg_rep_data{current_step_index, gg_rep_status};

    std::uint8_t connection_retries = 0;

    enum class Phase { Idle, StartReq, HandleRequest, WaitArmComplete, StopReq, EndReq };
    Phase phase = Phase::Idle;

    bool log_phase = true;

    while (gg->m_run_process)
    {
        switch (phase)
        {
            case Phase::Idle:
            {
                if (log_phase)
                {
                    std::cout << "IDLE" << std::endl;
                    log_phase = false;
                } 
                // Wait for server data
                auto rc = gg->m_scripted_motion_request_inet_handler->clientRead(&gg_req_data);
                if (rc < 0)
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    ++connection_retries;
                    if (connection_retries == 10)
                    {
                        std::cout << "Error while checking server connection. Waiting 10 seconds before retry." << std::endl;
                        std::this_thread::sleep_for(std::chrono::seconds(1));
                        connection_retries = 0;
                    }
                    break;
                }
                else if (rc == 0)
                {
                    break;
                }
                if (gg_req_data.step_status == ScriptedMotionStatus::START_REQUEST)
                {
                    if (gg_req_data.step_num == 0 && current_step_index != 0)
                        current_step_index = 0;
                    if (gg_req_data.step_num == current_step_index)
                    {
                        req_data.step_num = current_step_index;
                        req_data.step_status = ScriptedMotionStatus::START_REQUEST;
                        phase = Phase::HandleRequest;
                        log_phase = true;
                    }
                }
                else if (gg_req_data.step_status != ScriptedMotionStatus::START_REQUEST)
                {
                    current_step_index = 0;
                    req_data.step_num = current_step_index;
                    req_data.step_status = ScriptedMotionStatus::IDLE;
                    gg->m_scripted_motion_request_shmem_status->shmemWrite(&req_data);
                }
                break;
            }
            case Phase::HandleRequest:
            {
                if (log_phase)
                {
                    std::cout << "HANDLE_REQUEST" << std::endl;
                    log_phase = false;
                } 
                std::cout << "Handling request data. Step num: " << current_step_index << std::endl;
                if (gg->m_scripted_motion_step_inet_handler->clientRead(&servo_step) <= 0)
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(50));
                    break;
                }
                // First send step info, then request movement
                gg->m_scripted_motion_step_shmem_handler->shmemWrite(&servo_step);
                gg->m_scripted_motion_request_shmem_status->shmemWrite(&req_data);
                phase = Phase::WaitArmComplete;
                log_phase = true;
                break;
            }
            case Phase::WaitArmComplete:
            {
                if (log_phase)
                {
                    std::cout << "WAIT_ARM_COMPLETE" << std::endl;
                    log_phase = false;
                }
                // Check if request was cancelled or connection with server was lost
                auto rc = gg->m_scripted_motion_request_inet_handler->clientRead(&gg_req_data);
                if (rc < 0)
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    ++connection_retries;
                    if (connection_retries == 10)
                    {
                        std::cout << "Error while checking server connection. Waiting 10 seconds before retry." << std::endl;
                        std::this_thread::sleep_for(std::chrono::seconds(1));
                        connection_retries = 0;
                        current_step_index = 0;
                        phase = Phase::Idle;
                        log_phase = true;
                    }
                } 
                else if (rc == 0)
                {
                    // Do nothing
                }
                else
                {
                    if (gg_req_data.step_status != ScriptedMotionStatus::START_REQUEST)
                    {
                        current_step_index = 0;
                        phase = Phase::Idle;
                        log_phase = true;
                    }
                }
                gg->m_scripted_motion_reply_shmem_status->shmemRead(&rep_data);
                if (rep_data.step_num == current_step_index && rep_data.step_status == ScriptedMotionStatus::REQUEST_COMPLETED)
                {
                    gg_rep_data.step_num = current_step_index;
                    gg_rep_data.step_status = ScriptedMotionStatus::REQUEST_COMPLETED;
                    if (gg->m_scripted_motion_request_inet_handler->clientWrite(&gg_rep_data) < 0)
                    {
                        std::cout << "[GUI GATEWAY] Error: Couldn't write automated motion reply to server." << std::endl;
                        break;
                    }
                    ++current_step_index;
                    phase = Phase::Idle;
                    log_phase = true;
                    break;
                }
            }
            default:
                break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
}

void GuiGateway::signalCallbackHandler(int signum)
{
    TcpHandler<DiagnosticData>::signalCallbackHandler(signum);
    TcpHandler<OdinControlSelection>::signalCallbackHandler(signum);
    ShmemHandler<DiagnosticData>::signalCallbackHandler(signum);
    ShmemHandler<OdinControlSelection>::signalCallbackHandler(signum);
    std::cout << "GuiGateway received signal: " << signum << std::endl;
    m_run_process = false;
}

} // gui_gateway
} // odin