#ifndef SERVOMANAGER_H
#define SERVOMANAGER_H

#include "InetCommData.h"
#include "JoypadData.h"
#include "JoypadHandler.h"
#include "ServoController.h"
#include "odin/led_handler/LedHandler.h"
#include "odin/led_handler/DataTypes.h"
#include "odin/shmem_wrapper/ShmemHandler.hpp"

#include <array>
#include <string>
#include <cstdint>
#include <semaphore.h>
#include <vector>

// TODO: Move it to config file
enum LedOption
{
    JOYPAD,
    AUTOMATIC_READY,
    AUTOAMTIC_EXECUTE,
    IDLE,
    ERROR
};

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
    OdinControlSelection m_control_selection;
    OdinControlSelection m_previous_control_selection;
    std::string m_joypad_manager_pid;

    ws2811_led_t m_led_color_status[led_handler::LED_COUNT];

    std::uint8_t * m_data;

    bool m_automatic_movement_done = true;

    OdinServoStep m_automatic_servo_step;

    int m_current_servo_l = 1;
    int m_current_servo_r = 0;

    automatic_movement_status_t m_automatic_movement_status;

    std::unique_ptr<odin::shmem_wrapper::ShmemHandler<JoypadData>> m_joypad_shmem_handler;
    std::unique_ptr<odin::shmem_wrapper::ShmemHandler<ws2811_led_t>> m_led_shmem_handler;
    std::unique_ptr<odin::shmem_wrapper::ShmemHandler<OdinControlSelection>> m_control_selection_shmem_handler;
    std::unique_ptr<odin::shmem_wrapper::ShmemHandler<automatic_movement_status_t>> m_automatic_execute_shmem_handler;
    std::unique_ptr<odin::shmem_wrapper::ShmemHandler<OdinServoStep>> m_automatic_step_shmem_handler;

    // TODO: move those kind of values to some config file
    static constexpr int JOYPAD_CONTROL_DATA_BINS = 7;
private:
    void praseJoypadData();
    void handleAutomaticData();
    void updateLedColors(std::uint8_t led_options);
};

#endif // SERVOMANAGER_H