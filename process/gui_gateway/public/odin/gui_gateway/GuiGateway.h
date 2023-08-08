#ifndef GUIGATEWAY_H
#define GUIGATEWAY_H

#include "InetCommHandler.hpp"
#include "odin/shmem_wrapper/ShmemHandler.hpp"
#include "odin/diagnostic_handler/DataTypes.h"

#include <memory>

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

    static bool m_run_process;
    static void signalCallbackHandler(int signum);
private:
    std::unique_ptr<InetCommHandler<DiagnosticData>> m_comm_handler;
    std::unique_ptr<ShmemHandler<DiagnosticData>> m_shmem_handler;
    DiagnosticData m_remote_diagnostic;
};

} // gui_gateway
} // odin

#endif // GUIGATEWAY_H