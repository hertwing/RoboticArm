#ifndef INETCOMMDATA_H
#define INETCOMMDATA_H

#include <cstdint>
#include <filesystem>
#include <string>

// TODO: Set fixed IP on RPI boards
const std::string ROBOTIC_ARM_IP = "192.168.1.32";
const std::string ROBOTIC_GUI_IP = "192.168.1.41";

static constexpr std::uint16_t DIAGNOSTIC_SOCKET_PORT = 7071;
static constexpr std::uint16_t CONTROL_SELECTION_PORT = 7072;
static constexpr std::uint16_t AUTOMATIC_EXECUTION_PORT = 7073;
static constexpr std::uint16_t AUTOMATIC_SERVO_STEP_PORT = 7074;

typedef std::uint8_t automatic_movement_status_t;

// TODO: Rewrite to cinfig file
enum class ControlSelection
{
    NONE,
    JOYPAD,
    AUTOMATIC,
    DIAGNOSTIC
};

enum AutomaticMovementStatus
{
    NONE,
    START_SENDING,
    SEND_SUCCESS,
    SEND_DONE,
    RECEIVE_SUCCESS,
    RECEIVE_DONE,
    SEND_FAIL
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
    OdinServoStep()
    {
        step_num = -1;
        servo_num = 0;
        position = 0;
        delay = 0;
        speed = 0;
    }
    int step_num;
    std::uint8_t servo_num;
    std::uint16_t position;
    std::uint64_t delay;
    std::uint8_t speed;
};

#endif // INETCOMMDATA_H
