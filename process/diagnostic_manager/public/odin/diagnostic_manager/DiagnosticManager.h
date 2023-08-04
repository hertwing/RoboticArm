#ifndef DIAGNOSTICMANAGER_H
#define DIAGNOSTICMANAGER_H

#include "odin/diagnostic_handler/DiagnosticHandler.h"
#include <string>

using namespace odin::diagnostic_handler;

namespace odin
{
namespace diagnostic_manager
{

class DiagnosticManager
{
public:
    DiagnosticManager();
    ~DiagnosticManager() = default;

    void runProcess(int argc, char * argv[]);

    static bool m_run_process;
    static void signalCallbackHandler(int signum);
private:
    void diagnosticReader();

    DiagnosticHandler m_diagnostic_handler;
    std::string m_board_name;
};

} // diagnostic_manager
} // odin

#endif // DIAGNOSTICMANAGER_H