#include "ServoController.h"
#include "PCA9685.h"

#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include <unistd.h>

#include <cstdint>
#include <iostream>
#include <thread>
#include <chrono>

ServoController::ServoController() : m_current_position{0, 0}
{
    // Open I2C and set adress to PCA9685
    m_i2c_fd = ::open("/dev/i2c-1", O_RDWR);
    if (m_i2c_fd < 0) {
        std::cerr << "Error: cannot open /dev/i2c-1\n";
        std::exit(EXIT_FAILURE);
    }
    if (ioctl(m_i2c_fd, I2C_SLAVE, PCA9685_ADDR) < 0) {
        std::cerr << "Error: cannot set I2C addr 0x40\n";
        std::exit(EXIT_FAILURE);
    }

    // Reset and basic config
    // MODE1: Auto Increment on, no SLEEP
    if (!write8(m_i2c_fd, MODE1, AI)) {
        std::cerr << "Error: write MODE1 failed\n";
        std::exit(EXIT_FAILURE);
    }
    if (!write8(m_i2c_fd, MODE2, OUTDRV | OCH)) {
        std::cerr << "Error: write MODE2 failed\n";
        std::exit(EXIT_FAILURE);
    }

    // Set PWM frequency
    if (!setPWMFreq(m_i2c_fd, HERTZ)) {
        std::cerr << "Error: setPWMFreq failed\n";
        std::exit(EXIT_FAILURE);
    }

    // Zero all channels
    if (!allOff(m_i2c_fd)) {
        std::cerr << "Error: allOff failed\n";
        std::exit(EXIT_FAILURE);
    }

    setStartupPosition();
}

ServoController::~ServoController() {
    if (m_i2c_fd >= 0) {
        allOff(m_i2c_fd);
        ::close(m_i2c_fd);
    }
}

int ServoController::calcTicks(float impulseMs, int hertz)
{
    // 4096 kroków na okres
    float cycleMs = 1000.0f / hertz;
    int ticks = static_cast<int>(MAX_PWM * (impulseMs / cycleMs) + 0.5f);
    if (ticks < 0) ticks = 0;
    if (ticks > MAX_PWM) ticks = MAX_PWM;
    return ticks;
}

void ServoController::setStartupPosition()
{
    std::cout << "Setting ARM startup position..." << std::endl;
    for (int i = 0; i < SERVO_NUM; ++i)
    {
        float millis = static_cast<float>(STARTUP_POSITIONS[i]) / 1000.0f;
        std::cout << "Setting startup position for servo: " << i
                  << ", position: " << millis << " ms" << std::endl;
        int tick = calcTicks(millis, HERTZ);
        setTick(m_i2c_fd, static_cast<uint8_t>(i), static_cast<uint16_t>(tick));
        m_current_position[i] = STARTUP_POSITIONS[i];
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
    }
}

void ServoController::setAbsolutePosition(uint16_t position, uint8_t servo_num, uint8_t step /*=1*/)
{
    std::uint16_t current_position = m_current_position[servo_num];
    if (servo_num >= SERVO_NUM || current_position < MIN_POS_VAL || current_position > MAX_POS_VAL)
    {
        return;
    }
    std::cout << "Moving servo number: " << +servo_num
              << " to position: " << position
              << ". Step: " << +step << "." << std::endl;
    while (current_position != position)
    {
        if (current_position < position) {
            current_position = static_cast<uint16_t>(std::min<uint32_t>(position, current_position + step));
        } else {
            current_position = static_cast<uint16_t>(std::max<int>(position, current_position - step));
        }

        if ((servo_num < SERVO_NUM && current_position >= MIN_POS_VAL && current_position <= MAX_POS_VAL && servo_num != 1) || 
            (servo_num == 1 && current_position >= SERVO_1_MIN_POS_VAL && current_position <= SERVO_1_MAX_POS_VAL) ||
            (servo_num == 5 && current_position >= GRIPPER_MIN_POS_VAL && current_position <= GRIPPER_MAX_POS_VAL))
        {
            float millis = static_cast<float>(current_position) / 1000.0f;
            int tick = calcTicks(millis, HERTZ);
            setTick(m_i2c_fd, servo_num, static_cast<uint16_t>(tick));
            m_current_position[servo_num] = current_position;

            if (step < 3) {
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
            } else if (step < 6 && step >= 3) {
                std::this_thread::sleep_for(std::chrono::milliseconds(7));
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }
        else
        {
            current_position = position;
        }
    }
}

void ServoController::moveLeft(uint8_t servo_num, uint8_t value)
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

void ServoController::moveRight(uint8_t servo_num, uint8_t value)
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