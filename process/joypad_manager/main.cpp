#include "JoypadManager.h"

#include <signal.h>

int main()
{
    JoypadManager joypad_manager;
    signal(SIGINT, JoypadManager::signalCallbackHandler);
    joypad_manager.runProcess();
    return 0;
}