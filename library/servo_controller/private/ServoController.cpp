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
    for (int i = 0; i < SERVO_NUM; ++i)
    {
        float millis = static_cast<float>(STARTUP_POSITIONS[i]) / 1000;
        std::cout << "Setting startup position for servo: " << i << ", position: " << millis << std::endl;
        int tick = calcTicks(millis, HERTZ);
        pwmWrite(PIN_BASE + i, tick);
        m_current_position[i] = STARTUP_POSITIONS[i];
    }
}

void ServoController::setAbsolutePosition(int position, int servo_num)
{
    std::uint16_t current_position = m_current_position[servo_num];
    while (current_position != position)
    {
        if (current_position < position)
        {
            current_position += 3;
            if (current_position > position)
            {
                // std::cout << "debug" << std::endl;
                current_position = position;
            }
        } else {
            current_position -= 3;
            if (current_position < position)
            {
                // std::cout << "debug" << std::endl;
                current_position = position;
            }
        }
        if (servo_num >= 0 && servo_num < SERVO_NUM && current_position >= MIN_POS_VAL && current_position <= MAX_POS_VAL)
        {
            float millis = static_cast<float>(current_position) / 1000;
            // std::cout << "Setting absolute position for servo: " << servo_num << ", position: " << millis << std::endl;
            int tick = calcTicks(millis, HERTZ);
            pwmWrite(PIN_BASE + servo_num, tick);
            m_current_position[servo_num] = current_position;
            std::this_thread::sleep_for(std::chrono::microseconds(500));
        } else {
            current_position = position;
        }
    }
}

void ServoController::moveLeft(int servo_num)
{
    if (servo_num == 1)
    {
        setAbsolutePosition(m_current_position[servo_num] - 3, servo_num);
    }
    else if (servo_num == 0 || servo_num == 3)
    {
        setAbsolutePosition(m_current_position[servo_num] + 10, servo_num);
    }
    else 
    {
        setAbsolutePosition(m_current_position[servo_num] - 10, servo_num);
    }
}

void ServoController::moveRight(int servo_num)
{
    if (servo_num == 1)
    {
        setAbsolutePosition(m_current_position[servo_num] + 3, servo_num);
    }
    else if (servo_num == 0 || servo_num == 3)
    {
        setAbsolutePosition(m_current_position[servo_num] - 10, servo_num);
    }
    else
    {
        setAbsolutePosition(m_current_position[servo_num] + 10, servo_num);
    }
}