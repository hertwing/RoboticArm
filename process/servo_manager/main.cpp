#include "ServoManager.h"

#include <signal.h>

int main()
{
    ServoManager servo_manager;
    signal(SIGINT, ServoManager::signalCallbackHandler);
    servo_manager.runProcess();
    return 0;
}