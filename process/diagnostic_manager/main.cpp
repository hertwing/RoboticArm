#include "odin/diagnostic_manager/DiagnosticManager.h"

#include <signal.h>

using namespace odin::diagnostic_manager;

int main(int argc, char *argv[])
{
    DiagnosticManager diagnostic_manager;
    signal(SIGINT, DiagnosticManager::signalCallbackHandler);
    diagnostic_manager.runProcess(argc, argv);
    return 0;
}