#ifndef DIAGNOSTICHANDLER_DATATYPES_H
#define DIAGNOSTICHANDLER_DATATYPES_H

#include <cstdint>
#include <string>

namespace odin
{
namespace diagnostic_handler
{

static const std::string ARM_BOARD_NAME = "OdinArm";
static const std::string GUI_BOARD_NAME = "OdinGui";

static constexpr const char * CPU_USAGE_CMD = "top -n 1 -b | awk '/^%Cpu/{print $2}'";
static constexpr const char * CPU_TEMP_CMD = "vcgencmd measure_temp | egrep -o '[0-9]*\\.[0-9]*'";
static constexpr const char * TOTAL_MEM_CMD = "cat /proc/meminfo | grep MemTotal | awk '{print $2}'";
static constexpr const char * FREE_MEM_CMD = "cat /proc/meminfo | grep MemFree | awk '{print $2}'";
// TODO: Need to setup RPi IP manually and add boards addresses to hosts
static constexpr const char * LATENCY_GUI_CMD = "ping -c 1 192.168.1.24 | grep time= | awk -F'[a-z=&\"]*' '{print $7}'";
static constexpr const char * LATENCY_ARM_CMD = "ping -c 1 192.168.1.41 | grep time= | awk -F'[a-z=&\"]*' '{print $7}'";

struct DiagnosticData
{
    std::uint32_t cpu_usage = 0;
    std::uint32_t ram_usage = 0;
    std::uint32_t cpu_temp = 0;
    std::uint32_t latency = 0;
};

} // diagnostic_handler
} // odin

#endif // DIAGNOSTICHANDLER_DATATYPES_H