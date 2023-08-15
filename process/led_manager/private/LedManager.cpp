#include "LedManager.h"
#include "odin/led_handler/DataTypes.h"

#include <chrono>
#include <thread>
#include <iostream>

bool LedManager::m_run_process = true;

LedManager::LedManager() :
    m_led_handler()
{
}

void LedManager::runProcess()
{
    while(m_run_process)
    {
        m_led_handler.updateLedColors();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

void LedManager::signalCallbackHandler(int signum)
{
    LedHandler::signalCallbackHandler(signum);
    std::cout << "LedManager received signal: " << signum << std::endl;
    m_run_process = false;
}