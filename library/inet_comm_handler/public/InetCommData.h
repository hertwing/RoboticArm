#ifndef INETCOMMDATA_H
#define INETCOMMDATA_H

#include <cstdint>
#include <filesystem>
#include <string>

// TODO: Create a script to atumatically assing board IPs when env install
const std::string ROBOTIC_ARM_IP = "192.168.1.24";
const std::string ROBOTIC_GUI_IP = "192.168.1.41";

static constexpr std::uint16_t DIAGNOSTIC_SOCKET_PORT = 7071;
static constexpr std::uint16_t CONTROL_SELECTION_PORT = 7072;
static constexpr std::uint16_t AUTOMATIC_EXECUTION_PORT = 7073;
static constexpr std::uint16_t AUTOMATIC_SERVO_STEP_PORT = 7074;

// TODO: Rewrite to cinfig file
enum class ControlSelection
{
    NONE,
    JOYPAD,
    AUTOMATIC,
    DIAGNOSTIC
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

struct OdinAutomaticExecuteData
{
    std::uint8_t data_collection_status;
    bool run_in_loop;
};

struct OdinAutomaticConfirm
{
    bool confirm;
};

struct OdinAutomaticStepConfirm
{
    std::uint64_t step_num;
};

#endif // INETCOMMDATA_H
