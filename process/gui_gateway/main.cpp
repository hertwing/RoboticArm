#include "odin/gui_gateway/GuiGateway.h"

#include <signal.h>

using namespace odin::gui_gateway;

int main(int argc, char * argv[])
{
    GuiGateway gui_gateway;
    signal(SIGINT, GuiGateway::signalCallbackHandler);
    gui_gateway.runProcess(argc, argv);
    return 0;
}