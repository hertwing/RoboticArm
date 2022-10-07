#include "ServoController.h"

#include "pca9685.h"
#include <wiringPi.h>
#include <cstdlib>
#include <iostream>

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
        float millis = static_cast<float>(MIDDLE_POS_VAL) / 1000;
        std::cout << "Setting startup position for servo: " << i << ", position: " << millis << std::endl;
        int tick = calcTicks(millis, HERTZ);
        pwmWrite(PIN_BASE + i, tick);
        m_current_position[i] = MIDDLE_POS_VAL;
    }
}

void ServoController::setAbsolutePosition(int position, int servo_num)
{
    if (servo_num >= 0 && servo_num < SERVO_NUM && position >= MIN_POS_VAL && position <= MAX_POS_VAL)
    {
        float millis = static_cast<float>(position) / 1000;
        std::cout << "Setting absolute position for servo: " << servo_num << ", position: " << millis << std::endl;
        int tick = calcTicks(millis, HERTZ);
        pwmWrite(PIN_BASE + servo_num, tick);
        m_current_position[servo_num] = position;
    }
}