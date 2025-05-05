#ifndef INETCOMMDATA_H
#define INETCOMMDATA_H

#include <cstdint>
#include <filesystem>
#include <string>

// TODO: Set fixed IP on RPI boards
const std::string ROBOTIC_ARM_IP = "192.168.1.37";
const std::string ROBOTIC_GUI_IP = "192.168.1.41";

static constexpr std::uint16_t DIAGNOSTIC_SOCKET_PORT = 7071;
static constexpr std::uint16_t CONTROL_SELECTION_PORT = 7072;
static constexpr std::uint16_t SCRIPTED_MOTION_REQUEST_PORT = 7073;
static constexpr std::uint16_t SCRIPTED_MOTION_SERVO_DATA_PORT = 7074;

typedef std::uint8_t scripted_motion_status_t;

// TODO: Rewrite to cinfig file
enum class ControlSelection
{
    NONE,
    JOYPAD,
    AUTOMATIC,
    DIAGNOSTIC
};

enum class ScriptedMotionRequestStatus
{
    NONE,
    START_REQUEST,
    EXECUTE_ON_ARM,
    REQUEST_COMPLETE,
    STOP_REQUESTED
};

enum class ScriptedMotionReplyStatus
{
    NONE,
    WAITING,
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

struct ScriptedMotionStepStatus
{
    std::uint64_t step_num;
    scripted_motion_status_t step_status;
};

#endif // INETCOMMDATA_H
