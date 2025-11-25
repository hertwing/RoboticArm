#ifndef INETCOMMDATA_H
#define INETCOMMDATA_H

#include <cstdint>
#include <filesystem>
#include <string>

// wlan0
// static const std::string ROBOTIC_GUI_IP = "192.168.1.103";
// static const std::string ROBOTIC_ARM_IP = "192.168.1.104";

// eth0
static const std::string ROBOTIC_GUI_IP = "10.0.0.1";
static const std::string ROBOTIC_ARM_IP = "10.0.0.2";

static constexpr std::uint16_t DIAGNOSTIC_SOCKET_PORT = 7071;
static constexpr std::uint16_t CONTROL_SELECTION_PORT = 7072;
static constexpr std::uint16_t SCRIPTED_MOTION_REQUEST_PORT = 7073;
static constexpr std::uint16_t SCRIPTED_MOTION_SERVO_DATA_PORT = 7074;
static constexpr std::uint16_t VIDEO_PORT = 7075;
static constexpr std::uint16_t CAMERA_POS_PORT = 7076;
static constexpr std::uint16_t CAMERA_POS_READY_PORT = 7077;

typedef std::uint8_t scripted_motion_status_t;

static constexpr std::uint32_t UDP_BUFF = 4*1024*1024;

// TODO: Rewrite to cinfig file
enum class ControlSelection
{
    NONE,
    JOYPAD,
    AUTOMATIC,
    DIAGNOSTIC,
    CAMERA
};

enum class ScriptedMotionStatus
{
    IDLE,
    START_REQUEST,
    WAITING_DATA,
    IN_PROGRESS,
    EXECUTE_ON_ARM,
    REQUEST_COMPLETED,
    STOP_REQUESTED,
    ERROR
};

enum class ScriptedMotionRequestStatus
{
    IDLE,
    START_REQUEST,
    EXECUTE_ON_ARM,
    REQUEST_COMPLETE,
    STOP_REQUESTED,
    ERROR
};

enum class ScriptedMotionReplyStatus
{
    IDLE,
    WAITING_DATA,
    IN_PROGRESS,
    COMPLETED,
    ERROR,
    DISCONNECTED
};

struct OdinControlSelection
{
    std::uint8_t control_selection;

    bool operator==(const OdinControlSelection & obj) const
    {
        return control_selection == obj.control_selection;
    }

    bool operator!=(const OdinControlSelection & obj) const
    {
        return !(*this==obj);
    }

    OdinControlSelection & operator=(const OdinControlSelection & obj)
    {
        control_selection = obj.control_selection;
        return *this;
    }
};

// TODO: Move to different header 
struct OdinServoStep
{
    std::uint64_t step_num;
    std::uint8_t servo_num;
    std::uint16_t position;
    std::uint64_t delay;
    std::uint8_t speed;

    OdinServoStep & operator=(const OdinServoStep & obj)
    {
        step_num = obj.step_num;
        servo_num = obj.servo_num;
        position = obj.position;
        delay = obj.delay;
        speed = obj.speed;
        return *this;
    }

    bool operator==(const OdinServoStep & obj) const
    {
        return step_num == obj.step_num &&
               servo_num == obj.servo_num &&
               position == obj.position &&
               delay == obj.delay &&
               speed == obj.speed;
    }

    bool operator!=(const OdinServoStep & obj) const
    {
        return !(*this==obj);
    }
};

struct CameraPosData
{
    OdinServoStep pan_pos;
    OdinServoStep tilt_pos;
    bool target_smiling = false;

    CameraPosData & operator=(const CameraPosData & obj)
    {
        pan_pos = obj.pan_pos;
        tilt_pos = obj.tilt_pos;
        target_smiling = obj.target_smiling;
        return *this;
    }

    bool operator==(const CameraPosData & obj) const
    {
        return pan_pos == obj.pan_pos &&
               tilt_pos == obj.tilt_pos &&
               target_smiling == obj.target_smiling;
    }

    bool operator!=(const CameraPosData & obj) const
    {
        return !(*this==obj);
    }
};

struct CameraPosReadyData
{
    OdinServoStep pan_pos;
    OdinServoStep tilt_pos;
    bool camera_pos_ready = false;

    CameraPosReadyData & operator=(const CameraPosReadyData & obj)
    {
        pan_pos = obj.pan_pos;
        tilt_pos = obj.tilt_pos;
        camera_pos_ready = obj.camera_pos_ready;
        return *this;
    }

    bool operator==(const CameraPosReadyData & obj) const
    {
        return pan_pos == obj.pan_pos &&
               tilt_pos == obj.tilt_pos &&
               camera_pos_ready == obj.camera_pos_ready;
    }

    bool operator!=(const CameraPosReadyData & obj) const
    {
        return !(*this==obj);
    }
};

struct ScriptedMotionStepData
{
    std::uint64_t step_num;
    ScriptedMotionStatus step_status;
};


#endif // INETCOMMDATA_H
