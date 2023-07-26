#include "LedManager.h"

#include <signal.h>

int main()
{
    LedManager led_manager;
    signal(SIGINT, LedManager::signalCallbackHandler);
    led_manager.runProcess();
    return 0;
}