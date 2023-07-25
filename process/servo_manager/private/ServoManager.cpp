#include "ServoManager.h"
#include "ShmemWrapper/DataTypes.h"
#include "tanos/led_handler/DataTypes.h"

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

    m_shmem_handler = std::make_unique<ShmemWrapper::ShmemHandler<std::uint8_t>>(
        ShmemWrapper::DataTypes::JOYPAD_SHMEM_NAME, CONTROL_DATA_BINS, "", true, ShmemWrapper::DataTypes::JOYPAD_SEM_NAME, true);
    m_led_handler.setJoypadSelectionColor(m_current_servo_l, m_current_servo_r);
}

void ServoManager::servoDataReader()
{
    while (m_run_process)
    {
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
        m_led_handler.setJoypadSelectionColor(m_current_servo_l, m_current_servo_r);
    }
    if (m_joypad_data_types.leftBumper && m_joypad_data_previous.data[0] != 4)
    {
        ++m_current_servo_l;
        if (m_current_servo_l > 5)
        {
            --m_current_servo_l;
        }
        m_led_handler.setJoypadSelectionColor(m_current_servo_l, m_current_servo_r);
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