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
    m_automatic_execute_confirm_shmem_handler = std::make_unique<odin::shmem_wrapper::ShmemHandler<OdinAutomaticConfirm>>(
        odin::shmem_wrapper::DataTypes::AUTOMATIC_EXECUTE_GATEWAY_CONFIRM_SHMEM_NAME, sizeof(OdinAutomaticConfirm), true);
    m_automatic_step_confirm_shmem_handler = std::make_unique<odin::shmem_wrapper::ShmemHandler<OdinAutomaticStepConfirm>>(
        odin::shmem_wrapper::DataTypes::AUTOMATIC_EXECUTE_STEP_CONFIRM_SHMEM_NAME, sizeof(OdinAutomaticStepConfirm), true);
    m_automatic_execute_shmem_handler = std::make_unique<odin::shmem_wrapper::ShmemHandler<OdinAutomaticExecuteData>>(
        odin::shmem_wrapper::DataTypes::AUTOMATIC_EXECUTE_SHMEM_NAME, sizeof(OdinAutomaticExecuteData), false);
    m_automatic_step_shmem_handler = std::make_unique<odin::shmem_wrapper::ShmemHandler<OdinServoStep>>(
        odin::shmem_wrapper::DataTypes::AUTOMATIC_STEP_SHMEM_NAME, sizeof(OdinServoStep), false);
    m_joypad_shmem_handler = std::make_unique<odin::shmem_wrapper::ShmemHandler<JoypadData>>(
        odin::shmem_wrapper::DataTypes::JOYPAD_SHMEM_NAME, sizeof(JoypadData), false);
    m_led_shmem_handler = std::make_unique<odin::shmem_wrapper::ShmemHandler<ws2811_led_t>>(
        odin::shmem_wrapper::DataTypes::LED_SHMEM_NAME, sizeof(m_led_color_status), false);
    m_control_selection_shmem_handler = std::make_unique<odin::shmem_wrapper::ShmemHandler<OdinControlSelection>>(
        odin::shmem_wrapper::DataTypes::CONTROL_SELECT_SHMEM_NAME, sizeof(OdinControlSelection), false);
    m_previous_control_selection.control_selection = static_cast<std::uint8_t>(ControlSelection::NONE);
    m_control_selection.control_selection = static_cast<std::uint8_t>(ControlSelection::NONE);
    if (m_led_shmem_handler->openShmem())
    {
        updateLedColors(LedOption::IDLE);
    }
}

void ServoManager::servoDataReader()
{
    while (m_run_process)
    {
        if (m_joypad_shmem_handler->openShmem() && m_led_shmem_handler->openShmem() && m_control_selection_shmem_handler->openShmem())
        {
            if (m_control_selection_shmem_handler->shmemRead(&m_control_selection))
            {
                if (m_previous_control_selection.control_selection != m_control_selection.control_selection)
                {
                    std::cout << +(m_previous_control_selection.control_selection) << " " << +(m_control_selection.control_selection) << std::endl;
                    m_previous_control_selection.control_selection = m_control_selection.control_selection;
                    std::cout << "Control selection changed to: ";
                    switch(m_control_selection.control_selection)
                    {
                        case static_cast<std::uint8_t>(ControlSelection::JOYPAD):
                            updateLedColors(LedOption::JOYPAD);
                            std::cout << "JOYPAD." << std::endl;
                            break;
                        case static_cast<std::uint8_t>(ControlSelection::AUTOMATIC):
                            updateLedColors(LedOption::AUTOMATIC_READY);
                            std::cout << "AUTOMATIC." << std::endl;
                            break;
                        default:
                            updateLedColors(LedOption::IDLE);
                            std::cout << "NONE." << std::endl;
                            break;
                    }
                }
                if (m_control_selection.control_selection == static_cast<std::uint8_t>(ControlSelection::JOYPAD))
                {
                    if (m_joypad_shmem_handler->shmemRead(&m_joypad_data))
                    {
                        praseJoypadData();
                    }
                    else
                    {
                        std::cout << "Couldn't read from shmem." << std::endl;
                    }
                }
                else if (m_control_selection.control_selection == static_cast<std::uint8_t>(ControlSelection::AUTOMATIC))
                {
                    handleAutomaticData();
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                }
                else
                {
                    
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            else
            {
                std::cout << "Couldn't read from control selection shmem." << std::endl;
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
        if (m_current_servo_l == m_current_servo_r)
        {
            if (m_current_servo_r != 0)
            {
                --m_current_servo_l;
            }
            else
            {
                ++m_current_servo_l;
            }
        }
        if (m_current_servo_l < 0)
        {
            ++m_current_servo_l;
        }
        updateLedColors(LedOption::JOYPAD);
    }
    if (m_joypad_data_types.leftBumper && m_joypad_data_previous.data[0] != 4)
    {
        ++m_current_servo_l;
        if (m_current_servo_l == m_current_servo_r)
        {
            if (m_current_servo_r != 0)
            {
                ++m_current_servo_l;
            }
            else
            {
                --m_current_servo_l;
            }
        }
        if (m_current_servo_l > 5)
        {
            --m_current_servo_l;
        }
        updateLedColors(LedOption::JOYPAD);
    }
    if (m_joypad_data_types.rightTrigger && m_joypad_data_previous.data[0] != 2)
    {
        --m_current_servo_r;
        if (m_current_servo_r == m_current_servo_l)
        {
            if (m_current_servo_l != 0)
            {
                --m_current_servo_r;
            }
            else
            {
                ++m_current_servo_r;
            }
        }
        if (m_current_servo_r < 0)
        {
            ++m_current_servo_r;
        }
        updateLedColors(LedOption::JOYPAD);
    }
    if (m_joypad_data_types.rightBumper && m_joypad_data_previous.data[0] != 8)
    {
        ++m_current_servo_r;
        if (m_current_servo_r == m_current_servo_l)
        {
            if (m_current_servo_l != 5)
            {
                ++m_current_servo_r;
            }
            else
            {
                --m_current_servo_r;
            }
        }
        if (m_current_servo_r > 5)
        {
            --m_current_servo_r;
        }
        updateLedColors(LedOption::JOYPAD);
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
        if (m_current_servo_r == 5)
        {
            m_servo_controller.moveRight(m_current_servo_r, 255 - m_joypad_data_types.rightStickX);
        }
        else
        {
            m_servo_controller.moveLeft(m_current_servo_r, m_joypad_data_types.rightStickX);
        }
    }
    else if (m_joypad_data_types.rightStickX > 129)
    {
        if (m_current_servo_r == 5)
        {
            m_servo_controller.moveLeft(m_current_servo_r, m_joypad_data_types.rightStickX);
        }
        else
        {
            m_servo_controller.moveRight(m_current_servo_r, 255 - m_joypad_data_types.rightStickX);
        }
    }

    for (int i = 0; i < JoypadHandler::JOYPAD_CONTROL_DATA_BINS; ++i)
    {
        m_joypad_data_previous.data[i] = m_joypad_data.data[i];
    }
}

void ServoManager::updateLedColors(std::uint8_t led_options)
{
    switch(led_options)
    {
        case LedOption::JOYPAD:
            for (int i = 0; i < led_handler::LED_COUNT; ++i)
            {
                m_led_color_status[i] = led_handler::LED_COLOR_NONE;
            }
            m_led_color_status[m_current_servo_l] = led_handler::LED_COLOR_ORANGE;
            m_led_color_status[m_current_servo_r] = led_handler::LED_COLOR_PINK;

            m_led_shmem_handler->shmemWrite(m_led_color_status);
            break;
        case LedOption::AUTOMATIC_READY:
            for (int i = 0; i < led_handler::LED_COUNT; ++i)
            {
                m_led_color_status[i] = led_handler::LED_COLOR_ORANGE;
            }
            m_led_shmem_handler->shmemWrite(m_led_color_status);
            break;
        case LedOption::AUTOAMTIC_EXECUTE:
            for (int i = 0; i < led_handler::LED_COUNT; ++i)
            {
                m_led_color_status[i] = led_handler::LED_COLOR_GREEN;
            }
            m_led_shmem_handler->shmemWrite(m_led_color_status);
            break;
        case LedOption::ERROR:
            for (int i = 0; i < led_handler::LED_COUNT; ++i)
            {
                m_led_color_status[i] = led_handler::LED_COLOR_RED;
            }
            m_led_shmem_handler->shmemWrite(m_led_color_status);
            break;
        case LedOption::IDLE:
            for (int i = 0; i < led_handler::LED_COUNT; ++i)
            {
                m_led_color_status[i] = led_handler::LED_COLOR_LIGHTBLUE;
            }
            m_led_shmem_handler->shmemWrite(m_led_color_status);
            break;
        default:
            for (int i = 0; i < led_handler::LED_COUNT; ++i)
            {
                m_led_color_status[i] = led_handler::LED_COLOR_NONE;
            }
            m_led_shmem_handler->shmemWrite(m_led_color_status);
            break;
    }
}

void ServoManager::handleAutomaticData()
{
    OdinAutomaticExecuteData automatic_execute_data;
    OdinAutomaticConfirm automatic_confirm;
    OdinAutomaticStepConfirm servo_step_confirm;
    OdinServoStep servo_step;
    automatic_confirm.confirm = false;

    if (m_automatic_execute_shmem_handler->openShmem())
    {
        m_automatic_execute_shmem_handler->shmemRead(&automatic_execute_data);
        if (automatic_execute_data.data_collection_status == 1)
        {
            m_automatic_steps.clear();
            std::cout << "Data collection started." << std::endl;
            // Start data collection
            automatic_confirm.confirm = true;
            while (!m_automatic_execute_confirm_shmem_handler->shmemWrite(&automatic_confirm)){};
            std::cout << "Writing confirm." << std::endl;

            bool finish_reading = false;
            int step_num = 0;
            servo_step_confirm.step_num = 0;
            while (!finish_reading)
            {
                if (m_automatic_step_shmem_handler->openShmem())
                {
                    if (m_automatic_step_shmem_handler->shmemRead(&servo_step))
                    {
                        // std::cout << "servo_step.step_num: " << servo_step.step_num << std::endl;
                        if (servo_step.step_num == step_num)
                        {
                            std::cout << "Reading step." << std::endl;
                            servo_step_confirm.step_num = step_num;
                            ++step_num;
                            while (!m_automatic_step_confirm_shmem_handler->shmemWrite(&servo_step_confirm)){};
                            std::cout << "Writing step confirm." << std::endl;
                            std::cout << servo_step.step_num << std::endl;
                            std::cout << +servo_step.servo_num << std::endl;
                            std::cout << servo_step.position << std::endl;
                            std::cout << +servo_step.speed << std::endl;
                            std::cout << servo_step.delay << std::endl;
                            m_automatic_steps.emplace_back(servo_step);
                        }
                    }
                    else
                    {
                        std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    }
                }
                else
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                }
                if (m_automatic_execute_shmem_handler->openShmem())
                {
                    if (m_automatic_execute_shmem_handler->shmemRead(&automatic_execute_data))
                    {
                        if (automatic_execute_data.data_collection_status == 2)
                        {
                            finish_reading = true;
                            automatic_confirm.confirm = true;
                            std::cout << "Data collection finished." << std::endl;
                        }
                    }
                }
                else
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                }
            }
            bool finish = false;
            automatic_confirm.confirm = false;
            while (!finish)
            {
                if (m_automatic_execute_shmem_handler->openShmem())
                {
                    if (m_automatic_execute_shmem_handler->shmemRead(&automatic_execute_data))
                    {
                        if (automatic_execute_data.data_collection_status == 2)
                        {
                            finish = true;
                            automatic_confirm.confirm = true;
                        }
                        else
                        {
                            std::this_thread::sleep_for(std::chrono::milliseconds(100));
                        }
                    }
                }
                else
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                }
            }
            while (!m_automatic_execute_confirm_shmem_handler->shmemWrite(&automatic_confirm)){};
        }
    }

    if (m_automatic_steps.size())
    {
        updateLedColors(LedOption::AUTOAMTIC_EXECUTE);
        for (int i = 0; i < m_automatic_steps.size(); ++i)
        {
            m_servo_controller.setAbsolutePosition(m_automatic_steps.at(i).position, m_automatic_steps.at(i).servo_num-1, m_automatic_steps.at(i).speed);
            std::this_thread::sleep_for(std::chrono::milliseconds(m_automatic_steps.at(i).delay));
        }
        updateLedColors(LedOption::AUTOMATIC_READY);
    }
    if (automatic_execute_data.run_in_loop == false)
    {
        m_automatic_steps.clear();
    }
}

void ServoManager::runProcess()
{
    servoDataReader();
}

void ServoManager::signalCallbackHandler(int signum)
{
    odin::shmem_wrapper::ShmemHandler<JoypadData>::signalCallbackHandler(signum);
    odin::shmem_wrapper::ShmemHandler<ws2811_led_t>::signalCallbackHandler(signum);
    odin::shmem_wrapper::ShmemHandler<OdinControlSelection>::signalCallbackHandler(signum);
    std::cout << "ServoManager received signal: " << signum << std::endl;
    m_run_process = false;
}