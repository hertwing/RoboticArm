#ifndef JOYPADMANAGER_H
#define JOYPADMANAGER_H

#include "JoypadHandler.h"
#include <array>

class JoypadManager
{
public:
    JoypadManager();
    ~JoypadManager() = default;

    void runProcess();

    static bool m_run_process;
    static void signalCallbackHandler(int signum);
private:
    void joypadReader();

    JoypadHandler m_joypad_handler;
    std::array<unsigned char, JoypadHandler::GENESYS_BYTES_NUM> m_joypad_data;
};

#endif // JOYPADMANAGER_H