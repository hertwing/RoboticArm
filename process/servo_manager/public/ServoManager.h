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
#include <atomic>
#include <cstdint>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

// TODO: Move it to config file
enum LedOption
{
    JOYPAD,
    AUTOMATIC_READY,
    AUTOAMTIC_EXECUTE,
    CAMERA,
    IDLE,
    ERROR
};

class ServoManager
{
public:
    ServoManager();
    ~ServoManager();

    void runProcess();
    void servoDataReader();

    static bool m_run_process;
    static void signalCallbackHandler(int signum);
private:
    JoypadData m_joypad_data;
    JoypadData m_joypad_data_previous;
    JoypadState m_joypad_state;
    ServoController m_servo_controller;
    std::uint8_t m_control_selection;
    std::optional<ControlSelection> m_previous_control_selection;
    std::string m_joypad_manager_pid;

    ws2811_led_t m_led_color_status[led_handler::LED_COUNT];

    std::uint8_t * m_data;

    bool m_automatic_movement_done = true;

    OdinServoStep m_automatic_servo_step;

    int m_current_servo_l = 1;
    int m_current_servo_r = 0;

    scripted_motion_status_t m_scripted_motion_status;

    std::unique_ptr<odin::shmem_wrapper::ShmemHandler<JoypadData>> m_joypad_shmem_handler;
    std::unique_ptr<odin::shmem_wrapper::ShmemHandler<ws2811_led_t>> m_led_shmem_handler;
    std::unique_ptr<odin::shmem_wrapper::ShmemHandler<std::uint8_t>> m_control_selection_shmem_handler;
    std::unique_ptr<odin::shmem_wrapper::ShmemHandler<ScriptedMotionStepData>> m_scripted_motion_request_shmem_handler;
    std::unique_ptr<odin::shmem_wrapper::ShmemHandler<ScriptedMotionStepData>> m_scripted_motion_reply_shmem_handler;
    std::unique_ptr<odin::shmem_wrapper::ShmemHandler<OdinServoStep>> m_scripted_motion_step_shmem_handler;
    std::unique_ptr<odin::shmem_wrapper::ShmemHandler<CameraPosData>> m_camera_pos_shmem_handler;
    std::unique_ptr<odin::shmem_wrapper::ShmemHandler<CameraPosReadyData>> m_camera_pos_ready_shmem_handler;

    // TODO: move those kind of values to some config file
    static constexpr int JOYPAD_CONTROL_DATA_BINS = 7;

    // Automatic Data
    enum class Phase { Idle, StartReq, HandleReq, WaitStepCompleted, EndReq, StopReq };
    bool m_stop_requested = false;
    Phase m_phase = Phase::Idle;
    std::uint64_t m_current_step_index = 0;

    ScriptedMotionStatus m_req_status = ScriptedMotionStatus::IDLE;
    ScriptedMotionStatus m_rep_status = ScriptedMotionStatus::IDLE;

    ScriptedMotionStepData m_req_data{m_current_step_index, m_req_status};
    ScriptedMotionStepData m_rep_data{m_current_step_index, m_rep_status};

    CameraPosData m_camera_pos_data;
    CameraPosReadyData m_camera_pos_ready_data;

    bool m_log_phase = true;
private:
    void parseJoypadData();
    void handleControlSelectionChanged(ControlSelection selection);
    void handleCurrentControlSelection(ControlSelection selection);
    void handleJoypadControl();
    void handleAutomaticData();
    void handleCameraMovement();
    void updateLedColors(std::uint8_t led_options);
private:
    // TODO: Move somewhere else
    const std::vector<OdinServoStep> m_smile_wave = {
        {1,  0, 1500, 0, 10},
        {2,  2, 1670, 0, 10},
        {3,  1, 1500, 0, 10},
        {4,  3, 1400, 0, 10},
        {5,  5, 1500, 0, 10},
        {6,  4, 2500, 0, 10},
        {7,  3, 1750, 0, 10},
        {8,  3, 980,  0, 10},
        {9,  3, 1750, 0, 10},
        {10, 3, 980,  0, 10},
        {11, 3, 1400, 0, 10}
    };
    static void smileWaveMovement(ServoManager *);
    std::thread m_smile_wave_thread;
};

static std::atomic_bool smile_detected = false;
static std::atomic_bool smile_wave_in_progress = false;

#endif // SERVOMANAGER_H