#include "JoypadManager.h"

#include <chrono>
#include <thread>
#include <iostream>
#include<signal.h>

bool JoypadManager::m_run_process = true;

JoypadManager::JoypadManager() :
    m_joypad_handler()
{
}

void JoypadManager::joypadReader()
{
    while (m_run_process)
    {
        m_joypad_handler.connectAndRun();
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
}

void JoypadManager::runProcess()
{
    joypadReader();
}

void JoypadManager::signalCallbackHandler(int signum)
{
    JoypadHandler::signalCallbackHandler(signum);
    std::cout << "JoypadManager received signal: " << signum << std::endl;
    
    m_run_process = false;
}