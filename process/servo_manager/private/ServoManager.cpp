#include "ServoManager.h"

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
}

void ServoManager::servoDataReader()
{
    while (m_run_process)
    {
        if (readJoypadManagerPid())
        {
            m_is_shmem_opened = openJoypadShmem();
        }
        if (m_is_shmem_opened)
        {
            if (sem_wait(m_reader_sem) == -1)
            {
                std::cerr << "Couldn't wait reader semaphore." << std::endl;
            }
            if (sem_post(m_writer_sem) == -1)
            {
                std::cerr << "Couldn't post writer semaphore." << std::endl;
            }
            for (int i = 0; i < JoypadHandler::CONTROL_DATA_BINS; ++i)
            {
                m_joypad_data.data[i] = m_data[i];
            }
            if (sem_wait(m_writer_sem) == -1)
            {
                std::cerr << "Couldn't wait writer semaphore." << std::endl;
            }
            if (sem_post(m_reader_sem) == -1)
            {
                std::cerr << "Couldn't post reader semaphore." << std::endl;
            }
            praseJoypadData();
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
    munmap(m_data, JoypadHandler::CONTROL_DATA_BINS);
    close(m_joypad_shmem_fd);
    shm_unlink(JoypadShmemHandler::SHMEM_NAME);
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

bool ServoManager::readJoypadManagerPid()
{
    std::ifstream ifs(m_shmem_identifier);
    if (!ifs.is_open())
    {
        std::cerr << "failed to open " << m_shmem_identifier << '\n';
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    else
    {
        std::string obtained_pid;
        getline(ifs, obtained_pid);
        if (obtained_pid.compare(m_joypad_manager_pid) != 0)
        {
            // if previous shmem fd was opened, close it before opening another
            if (m_is_shmem_opened)
            {
                std::cout << "Shmem fd changed, closing previous one." << std::endl;
                munmap(m_data, JoypadHandler::CONTROL_DATA_BINS);
                close(m_joypad_shmem_fd);
                shm_unlink(JoypadShmemHandler::SHMEM_NAME);
            }
            m_joypad_manager_pid = obtained_pid;
            m_writer_sem_name = std::string(JoypadShmemHandler::WRITER_SEM_NAME) + m_joypad_manager_pid;
            m_reader_sem_name = std::string(JoypadShmemHandler::READER_SEM_NAME) + m_joypad_manager_pid;
            m_joypad_shmem_name = std::string(JoypadShmemHandler::SHMEM_NAME) + m_joypad_manager_pid;
            // TODO: move this
            if ((m_writer_sem = sem_open(m_writer_sem_name.c_str(), 0, 0, 0)) == SEM_FAILED)
            {
                std::cerr << "Couldn't create semaphore " + m_writer_sem_name << std::endl;
            }
            if ((m_reader_sem = sem_open(m_reader_sem_name.c_str(), 0, 0, 0)) == SEM_FAILED)
            {
                std::cerr << "Couldn't create semaphore " + m_reader_sem_name << std::endl;
            }
            return true;
        }
    }
    return false;
}

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