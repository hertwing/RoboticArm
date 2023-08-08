#include "ServoManager.h"
#include "odin/shmem_wrapper/DataTypes.h"
#include "odin/led_handler/DataTypes.h"

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
    m_joypad_shmem_handler = std::make_unique<odin::shmem_wrapper::ShmemHandler<JoypadData>>(
        odin::shmem_wrapper::DataTypes::JOYPAD_SHMEM_NAME, sizeof(JoypadData), false);
    m_led_shmem_handler = std::make_unique<odin::shmem_wrapper::ShmemHandler<ws2811_led_t>>(
        odin::shmem_wrapper::DataTypes::LED_SHMEM_NAME, sizeof(m_led_color_status), false);
    if (m_led_shmem_handler->openShmem())
    {
        updateLedColors();
    }
}

void ServoManager::servoDataReader()
{
    while (m_run_process)
    {
        if (m_joypad_shmem_handler->openShmem() && m_led_shmem_handler->openShmem())
        {
            if (m_joypad_shmem_handler->shmemRead(&m_joypad_data))
            {
                praseJoypadData();
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            else
            {
                std::cout << "Couldn't read from shmem." << std::endl;
            }
        }
        else
        {
            std::cout << "Couldn't open shmem." << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
}

void ServoManager::praseJoypadData()
{
    for (int i = 0; i < JoypadHandler::JOYPAD_CONTROL_DATA_BINS; ++i)
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
        updateLedColors();
    }
    if (m_joypad_data_types.leftBumper && m_joypad_data_previous.data[0] != 4)
    {
        ++m_current_servo_l;
        if (m_current_servo_l > 5)
        {
            --m_current_servo_l;
        }
        updateLedColors();
    }

    if (m_joypad_data_types.leftStickX < 125)
    {
        // TODO: Save servo numbers to config file
        if (m_current_servo_l == 5)
        {
            m_servo_controller.moveRight(m_current_servo_l, 255 - m_joypad_data_types.leftStickX);
        }
        else
        {
            m_servo_controller.moveLeft(m_current_servo_l, m_joypad_data_types.leftStickX);
        }
    } 
    else if (m_joypad_data_types.leftStickX > 129)
    {
        if (m_current_servo_l == 5)
        {
            m_servo_controller.moveLeft(m_current_servo_l, m_joypad_data_types.leftStickX);
        }
        else
        {
            m_servo_controller.moveRight(m_current_servo_l, 255 - m_joypad_data_types.leftStickX);
        }
    }
    if (m_joypad_data_types.rightStickX < 125)
    {
        m_servo_controller.moveLeft(0, m_joypad_data_types.rightStickX);
    }
    else if (m_joypad_data_types.rightStickX > 129)
    {
        m_servo_controller.moveRight(0, 255 - m_joypad_data_types.rightStickX);
    }

    for (int i = 0; i < JoypadHandler::JOYPAD_CONTROL_DATA_BINS; ++i)
    {
        m_joypad_data_previous.data[i] = m_joypad_data.data[i];
    }
}

void ServoManager::updateLedColors()
{
    for (int i = 0; i < led_handler::LED_COUNT; ++i)
    {
        m_led_color_status[i] = led_handler::LED_COLOR_NONE;
    }
    m_led_color_status[m_current_servo_l] = led_handler::LED_COLOR_ORANGE;
    m_led_color_status[m_current_servo_r] = led_handler::LED_COLOR_PINK;

    m_led_shmem_handler->shmemWrite(m_led_color_status);
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