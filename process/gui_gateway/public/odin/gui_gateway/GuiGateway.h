#ifndef GUIGATEWAY_H
#define GUIGATEWAY_H

#include "InetCommHandler.hpp"
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
    static void handleGuiAutomaticSteps(GuiGateway * gg);

    static void handleArmDiagnostic(GuiGateway * gg);
    static void handleArmControlSelection(GuiGateway * gg);
    static void handleArmAutomaticSteps(GuiGateway * gg);

    static bool m_run_process;
    static void signalCallbackHandler(int signum);
private:
    std::unique_ptr<InetCommHandler<DiagnosticData>> m_diagnostic_comm_handler;
    std::unique_ptr<ShmemHandler<DiagnosticData>> m_diagnostic_shmem_handler;
    std::unique_ptr<InetCommHandler<OdinControlSelection>> m_control_selection_comm_handler;
    std::unique_ptr<ShmemHandler<OdinControlSelection>> m_control_selection_shmem_handler;
    std::unique_ptr<ShmemHandler<OdinAutomaticExecuteData>> m_automatic_execute_shmem_handler;
    std::unique_ptr<ShmemHandler<OdinAutomaticConfirm>> m_automatic_execute_confirm_shmem_handler;
    std::unique_ptr<ShmemHandler<OdinAutomaticStepConfirm>> m_automatic_step_confirm_shmem_handler;
    std::unique_ptr<ShmemHandler<OdinServoStep>> m_automatic_step_shmem_handler;
    DiagnosticData m_remote_diagnostic;
    DiagnosticData m_previous_remote_diagnostic;
    OdinControlSelection m_control_selection;
    OdinControlSelection m_previous_control_selection;

    std::thread m_diagnostic_thread;
    std::thread m_control_selection_thread;
    std::thread m_automatic_data_thread;
};

} // gui_gateway
} // odin

#endif // GUIGATEWAY_H