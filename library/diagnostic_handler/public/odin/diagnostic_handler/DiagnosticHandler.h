#ifndef DIAGNOSTICHANDLER_H
#define DIAGNOSTICHANDLER_H

#include "odin/diagnostic_handler/DataTypes.h"
#include "odin/shmem_wrapper/ShmemHandler.hpp"

#include <cstdint>
#include <string>

namespace odin
{
namespace diagnostic_handler
{

class DiagnosticHandler
{
public:
    DiagnosticHandler();
    ~DiagnosticHandler() = default;

    std::string execCmd(const char* cmd);

    void getCpuUsage(std::uint32_t & data);
    void getRamUsage(std::uint32_t & data);
    void getCpuTemp(std::uint32_t & data);
    void getLatency(std::uint32_t & data, const std::string & board_name);

    void getDiagnosticData(const std::string & board_name);
    void writeDiagnostic(const std::string & board_name);

    void signalCallbackHandler(int signum);
private:
    std::unique_ptr<shmem_wrapper::ShmemHandler<DiagnosticData>> m_shmem_handler;
    std::uint32_t m_cpu_usage;
    std::uint32_t m_ram_usage;
    std::uint32_t m_cpu_temp;
    std::uint32_t m_latency;
    DiagnosticData m_diagnostic_data;
    static bool m_run_process;
};

} // diagnostic_handler
} // odin

#endif // DIAGNOSTICHANDLER_H
