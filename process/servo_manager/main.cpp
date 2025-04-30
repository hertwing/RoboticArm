#include "ServoManager.h"

#include <signal.h>

int main()
{
    signal(SIGPIPE, SIG_IGN);
    ServoManager servo_manager;
    signal(SIGINT, ServoManager::signalCallbackHandler);
    servo_manager.runProcess();
    return 0;
}