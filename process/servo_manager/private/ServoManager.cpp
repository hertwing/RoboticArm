#include "ServoManager.h"
#include "DataTypes.h"

#include <chrono>
#include <fstream>
#include <thread>
#include <iostream>
#include <signal.h>
#include <stdio.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <semaphore.h>

bool ServoManager::m_run_process = true;

ServoManager::ServoManager() :
    m_servo_controller()
{
    m_writer_sem_name = "";
    m_reader_sem_name = "";
    m_joypad_shmem_name = "";
    m_is_shmem_opened = false;

    m_shmem_handler = std::make_unique<ShmemHandler<std::uint8_t>>(DataTypes::JOYPAD_SHMEM_NAME, CONTROL_DATA_BINS, "", true, DataTypes::JOYPAD_SEM_NAME, true);
}

void ServoManager::servoDataReader()
{
    while (m_run_process)
    {
        // if (readJoypadManagerPid())
        if (m_shmem_handler->openShmem())
        {
            if (m_shmem_handler->shmemRead(m_joypad_data.data))
            {
                praseJoypadData();
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            else
            {
                std::cout << "Couldn't read from shmem." << std::endl;
            }
        }
    }
    // munmap(m_data, JoypadHandler::CONTROL_DATA_BINS);
    // close(m_joypad_shmem_fd);
    // shm_unlink(JoypadShmemHandler::SHMEM_NAME);
}

void ServoManager::praseJoypadData()
{
    for (int i = 0; i < JoypadHandler::CONTROL_DATA_BINS; ++i)
    {
        if (m_joypad_data_previous.data[i] != m_joypad_data.data[i])
        {
            m_joypad_data_types.parseJoypadData(m_joypad_data);
            break;
        }
    }

    if (m_joypad_data_types.leftTrigger && m_joypad_data_previous.data[0] != 1)
    {
        --m_current_servo_l;
        if (m_current_servo_l < 1)
        {
            ++m_current_servo_l;
        }
    }
    if (m_joypad_data_types.leftBumper && m_joypad_data_previous.data[0] != 4)
    {
        ++m_current_servo_l;
        if (m_current_servo_l > 5)
        {
            --m_current_servo_l;
        }
    }

    if (m_joypad_data_types.leftStickX < 125)
    {
        m_servo_controller.moveLeft(m_current_servo_l, m_joypad_data_types.leftStickX);
    } 
    else if (m_joypad_data_types.leftStickX > 129)
    {
        m_servo_controller.moveRight(m_current_servo_l, 255 - m_joypad_data_types.leftStickX);
    }
    if (m_joypad_data_types.rightStickX < 125)
    {
        m_servo_controller.moveLeft(0, m_joypad_data_types.rightStickX);
    }
    else if (m_joypad_data_types.rightStickX > 129)
    {
        m_servo_controller.moveRight(0, 255 - m_joypad_data_types.rightStickX);
    }

    for (int i = 0; i < JoypadHandler::CONTROL_DATA_BINS; ++i)
    {
        m_joypad_data_previous.data[i] = m_joypad_data.data[i];
    }
}

// bool ServoManager::readJoypadManagerPid()
// {
//     std::ifstream ifs(m_shmem_identifier);
//     if (!ifs.is_open())
//     {
//         std::cerr << "failed to open " << m_shmem_identifier << '\n';
//         std::this_thread::sleep_for(std::chrono::seconds(1));
//     }
//     else
//     {
//         std::string obtained_pid;
//         getline(ifs, obtained_pid);
//         if (obtained_pid.compare(m_joypad_manager_pid) != 0)
//         {
//             std::cout << "Reading Joypad Manager PID" << std::endl;
//             // if previous shmem fd was opened, close it before opening another
//             if (m_is_shmem_opened)
//             {
//                 m_shmem_handler.reset(nullptr);
//                 m_shmem_handler = std::make_unique<ShmemHandler<std::uint8_t>>(DataTypes::JOYPAD_SHMEM_NAME, CONTROL_DATA_BINS, nullptr, true, DataTypes::JOYPAD_SEM_NAME);
//                 // std::cout << "Shmem fd changed, closing previous one." << std::endl;
//                 // munmap(m_data, JoypadHandler::CONTROL_DATA_BINS);
//                 // close(m_joypad_shmem_fd);
//                 // shm_unlink(JoypadShmemHandler::SHMEM_NAME);
//             }
//             m_joypad_manager_pid = obtained_pid;
//             m_writer_sem_name = std::string("ControllerSem_writer_") + m_joypad_manager_pid;
//             m_reader_sem_name = std::string("ControllerSem_reader_") + m_joypad_manager_pid;
//             m_joypad_shmem_name = std::string(SHMEM_NAME) + m_joypad_manager_pid;
//             // TODO: move this
//             // if ((m_writer_sem = sem_open(m_writer_sem_name.c_str(), 0, 0, 0)) == SEM_FAILED)
//             // {
//             //     std::cerr << "Couldn't create semaphore " + m_writer_sem_name << std::endl;
//             // }
//             // if ((m_reader_sem = sem_open(m_reader_sem_name.c_str(), 0, 0, 0)) == SEM_FAILED)
//             // {
//             //     std::cerr << "Couldn't create semaphore " + m_reader_sem_name << std::endl;
//             // }
//             std::cout << "Creating shmem handler" << std::endl;

//             return true;
//         }
//     }
//     return false;
// }

bool ServoManager::openJoypadShmem()
{
    std::cout << "Opening shmem fd: " << m_joypad_shmem_name << std::endl;
    m_joypad_shmem_fd = shm_open(m_joypad_shmem_name.c_str(), O_RDONLY, 0666);
    if (m_joypad_shmem_fd < 0)
    {
        std::cerr << "Couldn't open joypad shmem." << std::endl;
        return false;
    }
    m_data = (std::uint8_t *)mmap(0, JoypadHandler::CONTROL_DATA_BINS, PROT_READ, MAP_SHARED, m_joypad_shmem_fd, 0);
    return true;
}

void ServoManager::runProcess()
{
    servoDataReader();
}

void ServoManager::signalCallbackHandler(int signum)
{
    std::cout << "ServoManager received signal: " << signum << std::endl;
    m_run_process = false;
}