#ifndef LEDMANAGER_H
#define LEDMANAGER_H

#include "odin/led_handler/LedHandler.h"
#include "odin/led_handler/DataTypes.h"

#include <array>
#include <string>
#include <cstdint>
#include <semaphore.h>

class LedManager
{
public:
    LedManager();
    ~LedManager() = default;

    void runProcess();

    static bool m_run_process;
    static void signalCallbackHandler(int signum);
private:
    LedHandler m_led_handler;
};

#endif // LEDMANAGER_H