#ifndef SHMEMHANDLER_DATATYPES_H
#define SHMEMHANDLER_DATATYPES_H

#include <filesystem>

namespace odin
{
namespace shmem_wrapper
{

struct DataTypes {
    static constexpr const char * SHMEM_IDENTIFIER_PATH = "/tmp/arm_shm/";
    static constexpr const char * SHMEM_IDENTIFIER_NAME = "_shmem_identifier.txt";


    static constexpr const char * SHMEM_READER_BLOCKER_NAME = "ReaderBlocker";
    static constexpr const char * SHMEM_WRITER_BLOCKER_NAME = "WriterBlocker";
    static constexpr const char * JOYPAD_SHMEM_NAME = "ControllerShmem";
    static constexpr const char * LED_SHMEM_NAME = "LedShmem";
    static constexpr const char * DIAGNOSTIC_SHMEM_NAME = "DiagnosticShmem";
    static constexpr const char * DIAGNOSTIC_FROM_REMOTE_SHMEM_NAME = "DiagnosticFromRemoteShmem";
    static constexpr const char * CONTROL_SELECT_SHMEM_NAME = "ControlSelectShmem";
    static constexpr const char * AUTOMATIC_EXECUTE_SHMEM_NAME = "AutomaticExecuteShmem";
    static constexpr const char * AUTOMATIC_EXECUTE_GATEWAY_CONFIRM_SHMEM_NAME = "AutomaticExecuteConfirmShmem";
    static constexpr const char * AUTOMATIC_EXECUTE_STEP_CONFIRM_SHMEM_NAME = "AutomaticStepConfirmShmem";
    static constexpr const char * AUTOMATIC_STEP_SHMEM_NAME = "AutomaticStepShmem";

    static constexpr const char * AUTOMATIC_FILES_PATH = "/odin/automatic_files";
};

} // shmem_wrapper
} // odin

#endif // SHMEMHANDLER_DATATYPES_H
