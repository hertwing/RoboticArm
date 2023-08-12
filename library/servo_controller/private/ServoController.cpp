#include "ServoController.h"

#include "pca9685.h"
#include <wiringPi.h>
#include <cstdlib>
#include <iostream>
#include <thread>
#include <chrono>

ServoController::ServoController() : m_current_position {0, 0}
{
    wiringPiSetup();

    // Setup with pinbase 300 and i2c location 0x40
    int fd = pca9685Setup(PIN_BASE, 0x40, HERTZ);
    if (fd < 0)
    {
        std::cerr <<  "Error in servo controller setup" << std::endl;
        exit(-1);
    }

    // Reset all output
    pca9685PWMReset(fd);

    setStartupPosition();
}

int ServoController::calcTicks(float impulseMs, int hertz)
{
    float cycleMs = 1000.0f / hertz;
    return (int)(MAX_PWM * impulseMs / cycleMs + 0.5f);
}

void ServoController::setStartupPosition()
{
    std::cout << "Setting ARM startup position..." << std::endl;
    for (int i = 0; i < SERVO_NUM; ++i)
    {
        float millis = static_cast<float>(STARTUP_POSITIONS[i]) / 1000;
        std::cout << "Setting startup position for servo: " << i << ", position: " << millis << std::endl;
        int tick = calcTicks(millis, HERTZ);
        pwmWrite(PIN_BASE + i, tick);
        m_current_position[i] = STARTUP_POSITIONS[i];
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
    }
}

void ServoController::setAbsolutePosition(int position, int servo_num, int step = 1)
{
    if (servo_num == 5)
    {
        if (position >= 2300)
        {
            return;
        } 
        else if (position <= 780)
        {
            return;
        }
    }
    std::cout << "Moving servo number: " << servo_num << " to position: " << position << ". Step: " << step << "." << std::endl;
    std::uint16_t current_position = m_current_position[servo_num];
    while (current_position != position)
    {
        if (current_position < position)
        {
            current_position += step;
            if (current_position > position)
            {
                current_position = position;
            }
        }
        else
        {
            current_position -= step;
            if (current_position < position)
            {
                current_position = position;
            }
        }
        if ((servo_num >= 0 && servo_num < SERVO_NUM && current_position >= MIN_POS_VAL && current_position <= MAX_POS_VAL && servo_num != 1) || 
            (servo_num == 1 && current_position >= SERVO_1_MIN_POS_VAL && current_position <= SERVO_1_MAX_POS_VAL))
        {
            float millis = static_cast<float>(current_position) / 1000;
            int tick = calcTicks(millis, HERTZ);
            pwmWrite(PIN_BASE + servo_num, tick);
            m_current_position[servo_num] = current_position;
            if (step < 3)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
            } else 
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            
        }
        else
        {
            current_position = position;
        }
    }
}

void ServoController::moveLeft(int servo_num, uint8_t value)
{
    if (servo_num == 1)
    {
        setAbsolutePosition(m_current_position[servo_num] - calculatePosition(value, 4), servo_num, calculatePosition(value, 4));
    }
    else if (servo_num == 0 || servo_num == 3)
    {
        setAbsolutePosition(m_current_position[servo_num] + calculatePosition(value, 10), servo_num, calculatePosition(value, 10));
    }
    else 
    {
        setAbsolutePosition(m_current_position[servo_num] - calculatePosition(value, 10), servo_num, calculatePosition(value, 10));
    }
}

void ServoController::moveRight(int servo_num, uint8_t value)
{
    if (servo_num == 1)
    {
        setAbsolutePosition(m_current_position[servo_num] + calculatePosition(value, 4), servo_num, calculatePosition(value, 4));
    }
    else if (servo_num == 0 || servo_num == 3)
    {
        setAbsolutePosition(m_current_position[servo_num] - calculatePosition(value, 10), servo_num, calculatePosition(value, 10));
    }
    else
    {
        setAbsolutePosition(m_current_position[servo_num] + calculatePosition(value, 10), servo_num, calculatePosition(value, 10));
    }
}

std::uint8_t ServoController::calculatePosition(uint8_t value, uint8_t max_step) const
{
    return (std::abs(127 - value) * max_step + 63) / 127;
}