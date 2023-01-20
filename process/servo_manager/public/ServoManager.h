#ifndef SERVOMANAGER_H
#define SERVOMANAGER_H

#include "ServoController.h"
#include "JoypadData.h"
#include "JoypadHandler.h"
#include "ShmemHandler.h"

#include <array>
#include <string>
#include <cstdint>
#include <semaphore.h>

class ServoManager
{
public:
    ServoManager();
    ~ServoManager() = default;

    void runProcess();
    void servoDataReader();

    static bool m_run_process;
    static void signalCallbackHandler(int signum);
private:
    JoypadData m_joypad_data;
    JoypadData m_joypad_data_previous;
    JoypadDataTypes m_joypad_data_types;
    ServoController m_servo_controller;
    std::string m_joypad_manager_pid;
    // TODO: Change paths definitions from ShmemHandler
    const std::string m_shmem_identifier = std::string(ShmemHandler<std::uint8_t>::SHMEM_IDENTIFIER_PATH) + "JoypadHandler" + std::string(ShmemHandler<std::uint8_t>::SHMEM_IDENTIFIER_NAME);

    std::string m_writer_sem_name;
    std::string m_reader_sem_name;
    std::string m_joypad_shmem_name;

    pid_t m_joypad_shmem_fd;
    bool m_is_shmem_opened;

    std::uint8_t * m_data;

    // sem_t * m_writer_sem;
    // sem_t * m_reader_sem;

    int m_current_servo_l = 1;
    int m_current_servo_r = 0;

    std::unique_ptr<ShmemHandler<std::uint8_t>> m_shmem_handler;

    static constexpr const char * SHMEM_NAME = "controller_shmem_";
    static constexpr int CONTROL_DATA_BINS = 7;
private:
    bool readJoypadManagerPid();
    bool openJoypadShmem();
    void praseJoypadData();
};

#endif // SERVOMANAGER_H