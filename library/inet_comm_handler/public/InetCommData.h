#ifndef INETCOMMDATA_H
#define INETCOMMDATA_H

#include <cstdint>
#include <string>
// #include "odin/diagnostic_handler/DataTypes.h"

// TODO: Create a script to atumatically assing board IPs when env instal
const std::string ROBOTIC_ARM_IP = "192.168.1.24";
const std::string ROBOTIC_GUI_IP = "192.168.1.41";

static constexpr std::uint16_t DIAGNOSTIC_SOCKET_PORT = 7071;

// struct OdinArmNetData
// {
//     odin::diagnostic_handler::DiagnosticData diagnostic_data;

// };

#endif // INETCOMMDATA_H