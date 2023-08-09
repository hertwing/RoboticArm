#ifndef INETCOMMDATA_H
#define INETCOMMDATA_H

#include <cstdint>
#include <string>
// #include "odin/diagnostic_handler/DataTypes.h"

// TODO: Create a script to atumatically assing board IPs when env instal
const std::string ROBOTIC_ARM_IP = "192.168.1.24";
const std::string ROBOTIC_GUI_IP = "192.168.1.41";

static constexpr std::uint16_t DIAGNOSTIC_SOCKET_PORT = 7071;
static constexpr std::uint16_t CONTROL_SELECTION_PORT = 7072;

// TODO: Rewrite to cinfig file
enum class ControlSelection
{
    JOYPAD,
    AUTOMATIC,
    DIAGNOSTIC,
    NONE
};

struct OdinControlSelection
{
    std::uint8_t control_selection;
};

#endif // INETCOMMDATA_H