#ifndef SERVOMANAGER_H
#define SERVOMANAGER_H

#include "ServoController.h"
#include "JoypadData.h"
#include "JoypadHandler.h"
#include "tanos/led_handler/LedHandler.h"
#include "tanos/shmem_wrapper/ShmemHandler.hpp"

#include <array>
#include <string>
#include <cstdint>
#include <semaphore.h>

class ServoManager
{
public:
    ServoManager();
    ~ServoManager() = default;

    void runProcess();
    void servoDataReader();

    static bool m_run_process;
    static void signalCallbackHandler(int signum);
private:
    JoypadData m_joypad_data;
    JoypadData m_joypad_data_previous;
    JoypadDataTypes m_joypad_data_types;
    ServoController m_servo_controller;
    LedHandler m_led_handler;
    std::string m_joypad_manager_pid;

    std::uint8_t * m_data;

    int m_current_servo_l = 1;
    int m_current_servo_r = 0;

    std::unique_ptr<shmem_wrapper::ShmemHandler<std::uint8_t>> m_shmem_handler;

    // TODO: move those kind of values to some config file
    static constexpr int CONTROL_DATA_BINS = 7;
private:
    void praseJoypadData();
};

#endif // SERVOMANAGER_H