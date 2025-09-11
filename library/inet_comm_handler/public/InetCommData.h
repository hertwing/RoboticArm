#ifndef INETCOMMDATA_H
#define INETCOMMDATA_H

#include <cstdint>
#include <filesystem>
#include <string>

static const std::string ROBOTIC_GUI_IP = "192.168.72.101";
static const std::string ROBOTIC_ARM_IP = "192.168.72.102";

static constexpr std::uint16_t DIAGNOSTIC_SOCKET_PORT = 7071;
static constexpr std::uint16_t CONTROL_SELECTION_PORT = 7072;
static constexpr std::uint16_t SCRIPTED_MOTION_REQUEST_PORT = 7073;
static constexpr std::uint16_t SCRIPTED_MOTION_SERVO_DATA_PORT = 7074;
static constexpr std::uint16_t VIDEO_PORT = 7075;

typedef std::uint8_t scripted_motion_status_t;

// TODO: Rewrite to cinfig file
enum class ControlSelection
{
    NONE,
    JOYPAD,
    AUTOMATIC,
    DIAGNOSTIC,
    CAMERA
};

enum class ScriptedMotionStatus
{
    IDLE,
    START_REQUEST,
    WAITING_DATA,
    IN_PROGRESS,
    EXECUTE_ON_ARM,
    REQUEST_COMPLETED,
    STOP_REQUESTED,
    ERROR
};

enum class ScriptedMotionRequestStatus
{
    IDLE,
    START_REQUEST,
    EXECUTE_ON_ARM,
    REQUEST_COMPLETE,
    STOP_REQUESTED,
    ERROR
};

enum class ScriptedMotionReplyStatus
{
    IDLE,
    WAITING_DATA,
    IN_PROGRESS,
    COMPLETED,
    ERROR,
    DISCONNECTED
};

struct OdinControlSelection
{
    bool operator!=(const OdinControlSelection & obj) const
    {
        if (control_selection == obj.control_selection)
        {
            return false;
        }
        return true;
    }

    OdinControlSelection & operator=(const OdinControlSelection & obj)
    {
        control_selection = obj.control_selection;
        return *this;
    }

    std::uint8_t control_selection;
};

struct OdinServoStep
{
    std::uint64_t step_num;
    std::uint8_t servo_num;
    std::uint16_t position;
    std::uint64_t delay;
    std::uint8_t speed;
};

struct ScriptedMotionStepData
{
    std::uint64_t step_num;
    ScriptedMotionStatus step_status;
};

// struct ScriptedMotionContext
// {
//     ScriptedMotionStepStatus local_request;
//     ScriptedMotionStepStatus local_reply;
//     ScriptedMotionStepStatus remote_request;
//     ScriptedMotionStepStatus remote_reply;
//     OdinServoStep servo_step_data;
//     std::uint64_t step_monitor = 0;
//     bool is_active = false;
// };

#endif // INETCOMMDATA_H
