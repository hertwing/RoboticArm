#ifndef SHMEM_DEFAULTS_HPP
#define SHMEM_DEFAULTS_HPP

#include <string_view>

namespace odin
{
namespace odin_comm
{

// inline constexpr std::string_view ControlSelectionShmemName = "/ControlSelectShmem_data";
// inline constexpr std::string_view ControlSelectAckShmemName = "/ControlSelectShmem_ack";

inline constexpr std::string_view ScriptedMotionFilesPath = "/odin/automatic_files";

inline constexpr std::string_view ShmemIdentifierPath = "/tmp/arm_shm/";
inline constexpr std::string_view ShmemIdentifierName = "_shmem_identifier.txt";

inline constexpr std::string_view ShmemReaderBlockerName = "ReaderBlocker";
inline constexpr std::string_view ShmemWriterBlockerName = "WriterBlocker";

inline constexpr std::string_view JoypadDataShmemName = "/ControllerShmem";
inline constexpr std::string_view LedShmemName = "/LedShmem";

inline constexpr std::string_view DiagnosticShmemName = "/DiagnosticShmem";
inline constexpr std::string_view DiagnosticFromRemoteShmemName = "/DiagnosticFromRemoteShmem";

inline constexpr std::string_view ControlSelectionShmemName = "/ControlSelectionShmem";

inline constexpr std::string_view ScriptedMotionCommandShmemName =
    "/ScriptedMotionCommandShmem";

inline constexpr std::string_view ScriptedMotionStatusShmemName =
    "/ScriptedMotionStatusShmem";

inline constexpr std::string_view ServoStepShmemName =
    "/ServoStepShmem";

inline constexpr std::string_view CameraPositionShmemName = "/CameraPositionShmem";
inline constexpr std::string_view CameraPositionReadyShmemName = "/CameraPositionReadyShmem";

} // namespace odin_comm
} // namespace odin

#endif // SHMEM_DEFAULTS_HPP