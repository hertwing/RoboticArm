#include "odin/gui_gateway/GuiGateway.h"

#include <signal.h>

using namespace odin::gui_gateway;

int main(int argc, char * argv[])
{
    if (argc < 2) {
        std::cout << "Usage: " << argv[0] << " <BOARD_NAME> " << std::endl;
        return -1;
    }
    signal(SIGPIPE, SIG_IGN);
    GuiGateway gui_gateway;
    signal(SIGINT, GuiGateway::signalCallbackHandler);
    gui_gateway.runProcess(argc, argv);
    return 0;
}