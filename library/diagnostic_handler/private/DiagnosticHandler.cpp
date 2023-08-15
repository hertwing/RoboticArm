#include "odin/diagnostic_handler/DiagnosticHandler.h"
#include "odin/diagnostic_handler/DataTypes.h"

#include <cstdio>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <array>

namespace odin
{
namespace diagnostic_handler
{

bool DiagnosticHandler::m_run_process = true;

DiagnosticHandler::DiagnosticHandler():
    m_cpu_usage(0),
    m_ram_usage(0),
    m_cpu_temp(0),
    m_latency(0)
{
    m_shmem_handler = std::make_unique<odin::shmem_wrapper::ShmemHandler<DiagnosticData>>(
        odin::shmem_wrapper::DataTypes::DIAGNOSTIC_SHMEM_NAME, sizeof(DiagnosticData), true);
}

std::string DiagnosticHandler::execCmd(const char * cmd)
{
    std::array<char, 128> buffer;
    std::string result;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);
    if (!pipe) {
        throw std::runtime_error("popen() failed!");
    }
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    return result;
}

void DiagnosticHandler::getCpuUsage(std::uint32_t & data)
{
    try
    {
        data = stoi(execCmd(CPU_USAGE_CMD));
    }
    catch(std::exception & error)
    {
        std::cerr << "Couldn't retrieve CPU usage: " << error.what() << std::endl;
    }
}

void DiagnosticHandler::getRamUsage(std::uint32_t & data)
{
    std::uint32_t ram_overall = 0;
    std::uint32_t ram_free = 0;
    std::uint32_t ram_in_use = 0;

    try
    {
        ram_overall = stoi(execCmd(TOTAL_MEM_CMD));
        ram_free = stoi(execCmd(FREE_MEM_CMD));
        ram_in_use = ram_overall - ram_free;
        data = (ram_in_use * 100) / ram_overall;
    }
    catch(const std::exception & error)
    {
        std::cerr << "Couldn't retrieve RAM usage: " << error.what() << std::endl;
    }
}

void DiagnosticHandler::getCpuTemp(std::uint32_t & data)
{
    try
    {
        data = stoi(execCmd(CPU_TEMP_CMD));
    }
    catch(const std::exception & error)
    {
        std::cerr << "Couldn't retrieve cpu temp: " << error.what() << std::endl;
    }
}

void DiagnosticHandler::getLatency(double & data, const std::string & board_name)
{
    try
    {
        board_name == ARM_BOARD_NAME ? data = std::stod(execCmd(LATENCY_ARM_CMD)) : data = std::stod(execCmd(LATENCY_GUI_CMD));
    }
    catch(const std::exception & error)
    {
        std::cerr << "Couldn't retrieve cpu temp: " << error.what() << std::endl;
    }
}

void DiagnosticHandler::getDiagnosticData(const std::string & board_name)
{
    getCpuUsage(m_cpu_usage);
    getRamUsage(m_ram_usage);
    getCpuTemp(m_cpu_temp);
    getLatency(m_latency, board_name);
}

void DiagnosticHandler::writeDiagnostic(const std::string & board_name)
{
    if(m_run_process)
    {
        getDiagnosticData(board_name);
        m_diagnostic_data.cpu_usage = m_cpu_usage;
        m_diagnostic_data.ram_usage = m_ram_usage;
        m_diagnostic_data.cpu_temp = m_cpu_temp;
        m_diagnostic_data.latency = m_latency;
        std::cout << "CPU usage: " << m_cpu_usage << std::endl;
        std::cout << "RAM usage: " << m_ram_usage << std::endl;
        std::cout << "CPU temp: " << m_cpu_temp << std::endl;
        std::cout << "Latency: " << m_latency << std::endl;
        m_shmem_handler->shmemWrite(&m_diagnostic_data);
    }
}

void DiagnosticHandler::signalCallbackHandler(int signum)
{
    odin::shmem_wrapper::ShmemHandler<DiagnosticData>::signalCallbackHandler(signum);
    std::cout << "DiagnosticHandler received signal: " << signum << std::endl;
    m_run_process = false;
}

} // diagnostic_handler
} // odin
