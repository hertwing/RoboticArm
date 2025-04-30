#include "JoypadManager.h"

#include <signal.h>

int main()
{
    signal(SIGPIPE, SIG_IGN);
    JoypadManager joypad_manager;
    signal(SIGINT, JoypadManager::signalCallbackHandler);
    joypad_manager.runProcess();
    return 0;
}