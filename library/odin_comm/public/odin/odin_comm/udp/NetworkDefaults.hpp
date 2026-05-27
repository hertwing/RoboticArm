#ifndef NETWORK_DEFAULTS_HPP
#define NETWORK_DEFAULTS_HPP

#include <cstdint>
#include <string_view>

namespace odin
{
namespace odin_comm
{

inline constexpr std::string_view RoboticGuiIp = "10.0.0.1";
inline constexpr std::string_view RoboticArmIp = "10.0.0.2";

inline constexpr std::uint16_t DiagnosticSocketPort = 7071;
inline constexpr std::uint16_t ControlSelectionPort = 7072;
inline constexpr std::uint16_t ScriptedMotionCommandPort = 7073;
inline constexpr std::uint16_t ScriptedMotionStatusPort = 7074;
inline constexpr std::uint16_t ServoStepPort = 7075;
inline constexpr std::uint16_t VideoPort = 7076;
inline constexpr std::uint16_t CameraPositionPort = 7077;
inline constexpr std::uint16_t CameraPositionReadyPort = 7078;
inline constexpr std::uint16_t JoypadDataPort = 7079;
inline constexpr std::uint16_t LedStatePort = 7080;

} // namespace odin_comm
} // namespace odin

#endif // NETWORK_DEFAULTS_HPP