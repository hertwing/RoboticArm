#ifndef GUIGATEWAY_H
#define GUIGATEWAY_H

#include "UdpHandler.hpp"
#include "TcpHandler.hpp"
#include "InetCommData.h"
#include "odin/shmem_wrapper/ShmemHandler.hpp"
#include "odin/diagnostic_handler/DataTypes.h"

#include <memory>
#include <thread>

using namespace odin::shmem_wrapper;
using namespace odin::diagnostic_handler;

namespace odin
{
namespace gui_gateway
{

class GuiGateway
{
public:
    GuiGateway();
    ~GuiGateway() = default;

    void runProcess(int argc, char * argv[]);

    void runOnGui();
    void runOnArm();

    static void handleGuiDiagnostic(GuiGateway * gg);
    static void handleGuiControlSelection(GuiGateway * gg);
    static void handleGuiScriptedMotionRequest(GuiGateway * gg);
    static void handleGuiCameraPos(GuiGateway * gg);

    static void handleArmDiagnostic(GuiGateway * gg);
    static void handleArmControlSelection(GuiGateway * gg);
    static void handleArmScriptedMotionRequest(GuiGateway * gg);
    static void handleArmCameraPos(GuiGateway * gg);

    static bool m_run_process;
    static void signalCallbackHandler(int signum);
private:
    std::unique_ptr<TcpHandler<DiagnosticData>> m_diagnostic_comm_handler;
    std::unique_ptr<ShmemHandler<DiagnosticData>> m_diagnostic_shmem_handler;

    std::unique_ptr<TcpHandler<std::uint8_t>> m_control_selection_comm_handler;
    std::unique_ptr<ShmemHandler<std::uint8_t>> m_control_selection_shmem_handler;

    std::unique_ptr<TcpHandler<ScriptedMotionStepData>> m_scripted_motion_request_inet_handler;
    std::unique_ptr<ShmemHandler<ScriptedMotionStepData>> m_scripted_motion_request_shmem_status;
    std::unique_ptr<ShmemHandler<ScriptedMotionStepData>> m_scripted_motion_reply_shmem_status;

    std::unique_ptr<TcpHandler<OdinServoStep>> m_scripted_motion_step_inet_handler;
    std::unique_ptr<ShmemHandler<OdinServoStep>> m_scripted_motion_step_shmem_handler;

    std::unique_ptr<UdpHandler<CameraPosData>> m_camera_pos_inet_handler;
    std::unique_ptr<TcpHandler<CameraPosReadyData>> m_camera_pos_ready_inet_handler;
    std::unique_ptr<odin::shmem_wrapper::ShmemHandler<CameraPosData>> m_camera_pos_shmem_handler;
    std::unique_ptr<odin::shmem_wrapper::ShmemHandler<CameraPosReadyData>> m_camera_pos_ready_shmem_handler;

    DiagnosticData m_remote_diagnostic;
    DiagnosticData m_previous_remote_diagnostic;

    std::uint8_t m_control_selection;
    std::uint8_t m_previous_control_selection;

    std::thread m_diagnostic_thread;
    std::thread m_control_selection_thread;
    std::thread m_scripted_motion_request_thread;
    std::thread m_camera_pos_thread;
};

} // gui_gateway
} // odin

#endif // GUIGATEWAY_H