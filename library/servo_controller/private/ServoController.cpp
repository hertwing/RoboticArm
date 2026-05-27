#include "ServoController.h"
#include "PCA9685.h"

#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include <unistd.h>

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <thread>
#include <chrono>
#include <cmath>

ServoController::ServoController() : m_current_position{0, 0}
{
    // Open I2C and set adress to PCA9685
    m_i2c_fd = ::open("/dev/i2c-1", O_RDWR);
    if (m_i2c_fd < 0)
    {
        std::cerr << "Error: cannot open /dev/i2c-1\n";
        std::exit(EXIT_FAILURE);
    }
    if (ioctl(m_i2c_fd, I2C_SLAVE, PCA9685_ADDR) < 0)
    {
        std::cerr << "Error: cannot set I2C addr 0x40\n";
        std::exit(EXIT_FAILURE);
    }

    // Reset and basic config
    // MODE1: Auto Increment on, no SLEEP
    if (!write8(m_i2c_fd, MODE1, AI))
    {
        std::cerr << "Error: write MODE1 failed\n";
        std::exit(EXIT_FAILURE);
    }
    if (!write8(m_i2c_fd, MODE2, OUTDRV | OCH))
    {
        std::cerr << "Error: write MODE2 failed\n";
        std::exit(EXIT_FAILURE);
    }

    // Set PWM frequency
    if (!setPWMFreq(m_i2c_fd, HERTZ))
    {
        std::cerr << "Error: setPWMFreq failed\n";
        std::exit(EXIT_FAILURE);
    }

    print_pca_report(m_i2c_fd, HERTZ /*=50*/, /*tol_pct=*/10.0);

    // Zero all channels
    if (!allOff(m_i2c_fd))
    {
        std::cerr << "Error: allOff failed\n";
        std::exit(EXIT_FAILURE);
    }

    setStartupPosition();
    // cameraMoveTest();
}

ServoController::~ServoController()
{
    if (m_i2c_fd >= 0)
    {
        allOff(m_i2c_fd);
        ::close(m_i2c_fd);
    }
}

std::uint16_t ServoController::calcTicks(double impulseMs)
{
    if (!std::isfinite(impulseMs)) return 0;
    const double cycleMs = 1000.0 / static_cast<double>(HERTZ);
    impulseMs = std::clamp(impulseMs, 0.0, cycleMs);
    const double frac = impulseMs / cycleMs;
    int ticks = static_cast<int>(std::lround(frac * (MAX_PWM + 1)));
    if (ticks < 0) ticks = 0;
    else if (ticks > MAX_PWM) ticks = MAX_PWM;
    return static_cast<uint16_t>(ticks);
}

void ServoController::setStartupPosition()
{
    std::cout << "Setting ARM startup position..." << std::endl;
    for (int i = 0; i < SERVO_NUM; ++i)
    {
        double ms = static_cast<double>(STARTUP_POSITIONS[i]) / 1000.0;
        std::cout << "Setting startup position for servo: " << i
                  << ", position: " << ms << " ms" << std::endl;
        int tick = calcTicks(ms);
        setTick(m_i2c_fd, static_cast<uint8_t>(i), static_cast<uint16_t>(tick));
        m_current_position[i] = STARTUP_POSITIONS[i];
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
}

void ServoController::setAbsolutePosition(std::uint16_t target_pos, std::uint8_t servo_num, std::uint8_t step)
{
    if (servo_num >= SERVO_NUM) return;
    if (m_current_position[servo_num] == target_pos) return;
    if (step == 0) step = 1;
    if (step > 10) step = 10;

    std::uint16_t min_val = MIN_POS_VAL;
    std::uint16_t max_val = MAX_POS_VAL;

    if (servo_num == SERVO_ARM_1)
    {
        min_val = SERVO_1_MIN_POS_VAL;
        max_val = SERVO_1_MAX_POS_VAL;
    }
    else if (servo_num == SERVO_GRIPPER)
    {
        min_val = GRIPPER_MIN_POS_VAL;
        max_val = GRIPPER_MAX_POS_VAL;
    }
    else if (servo_num == SERVO_CAMERA_PAN)
    {
        min_val = MIN_CAM_PAN_POS_VAL;
        max_val = MAX_CAM_PAN_POS_VAL;
    }

    std::int32_t target_pos_i = static_cast<std::int32_t>(target_pos);
    std::int32_t current_pos_i = static_cast<std::int32_t>(m_current_position[servo_num]);

    target_pos_i = std::clamp(target_pos_i, static_cast<std::int32_t>(min_val), static_cast<std::int32_t>(max_val));

    std::cout << "Moving servo number: " << +servo_num
              << " to position: " << target_pos_i
              << " from position: " << current_pos_i
              << " with step: " << +step << std::endl;
    using clock = std::chrono::steady_clock;

    // Speed and "step per frame" (20 ms @ 50 Hz)
    const float v_us_s = SPEED_PER_STEP * static_cast<float>(step);
    const int per_frame_us = std::max(1, static_cast<int>(std::ceil(v_us_s * 0.020f)));
    const int abs_dist_us  = std::abs(target_pos_i - current_pos_i);

    // FAST-PATH:
    if (abs_dist_us <= std::max<int>(EPS_US, per_frame_us))
    {
        current_pos_i = target_pos_i;
        double ms = static_cast<double>(current_pos_i) / 1000.0;
        std::uint16_t tick = calcTicks(ms);
        if (static_cast<std::uint16_t>(current_pos_i) != m_current_position[servo_num])
        {
            setTick(m_i2c_fd, servo_num, tick);
            m_current_position[servo_num] = static_cast<std::uint16_t>(current_pos_i);
        }
        return;
    }

    auto last   = clock::now();
    auto next_t = last + FRAME;
    std::uint16_t last_written = m_current_position[servo_num];

    while (current_pos_i != target_pos_i)
    {
        // time since previous iteration
        auto now = clock::now();
        float dt = std::chrono::duration<float>(now - last).count();
        if (dt <= 0.0f) dt = 0.02f; // fallback ~20 ms
        last = now;

        // distance and direction
        std::int32_t dist = target_pos_i - current_pos_i;
        int sign = (dist > 0) ? 1 : -1;
        int abs_dist = std::abs(dist);

        int step_us = static_cast<int>(std::ceil(v_us_s * dt));
        if (step_us < 1) step_us = 1;
        if (step_us > abs_dist) step_us = abs_dist;

        current_pos_i += sign * step_us;

        std::uint16_t new_us = static_cast<std::uint16_t>(current_pos_i);
        const int delta = std::abs(static_cast<int>(current_pos_i) - static_cast<int>(last_written));
        if (delta >= MIN_WRITE_DELTA_US)
        {
            double ms = static_cast<double>(new_us) / 1000.0;
            std::uint16_t tick = calcTicks(ms);
            setTick(m_i2c_fd, servo_num, tick);
            m_current_position[servo_num] = new_us;
            last_written = new_us;
        }

        // keep 50 Hz
        next_t += FRAME;
        auto now2 = clock::now();
        if (next_t < now2 - FRAME)
        {
            next_t = now2 + FRAME;
        }

        std::this_thread::sleep_until(next_t);
    }
}

void ServoController::moveLeft(uint8_t servo_num, uint8_t value)
{
    if (servo_num == SERVO_BASE || servo_num == SERVO_ARM_2)
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
    if (servo_num == SERVO_BASE || servo_num == SERVO_ARM_2)
    {
        setAbsolutePosition(m_current_position[servo_num] - calculatePosition(value, 10), servo_num, calculatePosition(value, 10));
    }
    else
    {
        setAbsolutePosition(m_current_position[servo_num] + calculatePosition(value, 10), servo_num, calculatePosition(value, 10));
    }
}

void ServoController::cameraMoveLeft(std::uint8_t servo_num, std::uint8_t value)
{
    if (servo_num != SERVO_CAMERA_PAN) return;
    setAbsolutePosition(m_current_position[servo_num] + value, servo_num, value);
}

void ServoController::cameraMoveRight(std::uint8_t servo_num, std::uint8_t value)
{
    if (servo_num != SERVO_CAMERA_PAN) return;
    setAbsolutePosition(m_current_position[servo_num] - value, servo_num, value);
}

void ServoController::cameraMoveUp(std::uint8_t servo_num, std::uint8_t value)
{
    if (servo_num != SERVO_CAMERA_TILT) return;
    setAbsolutePosition(m_current_position[servo_num] + value, servo_num, value);
}

void ServoController::cameraMoveDown(std::uint8_t servo_num, std::uint8_t value)
{
    if (servo_num != SERVO_CAMERA_TILT) return;
    setAbsolutePosition(m_current_position[servo_num] - value, servo_num, value);
}

void ServoController::cameraMoveTest()
{
    setAbsolutePosition(MIN_CAM_PAN_POS_VAL, SERVO_CAMERA_PAN, 5);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    setAbsolutePosition(MAX_CAM_PAN_POS_VAL, SERVO_CAMERA_PAN, 5);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    setAbsolutePosition(MIDDLE_POS_CAMERA_SERVO_PAN, SERVO_CAMERA_PAN, 5);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    // setAbsolutePosition(MIN_CAM_TILT_POS_VAL, SERVO_CAMERA_TILT, 5);
    // std::this_thread::sleep_for(std::chrono::milliseconds(500));
    // setAbsolutePosition(MAX_CAM_TILT_POS_VAL, SERVO_CAMERA_TILT, 5);
    // std::this_thread::sleep_for(std::chrono::milliseconds(500));
    // setAbsolutePosition(MIDDLE_POS_CAMERA_SERVO_TILT, SERVO_CAMERA_TILT, 5);
    // std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // setAbsolutePosition(MIN_CAM_PAN_POS_VAL, SERVO_CAMERA_PAN, 1);
    // std::this_thread::sleep_for(std::chrono::milliseconds(500));
    // setAbsolutePosition(MAX_CAM_PAN_POS_VAL, SERVO_CAMERA_PAN, 1);
    // std::this_thread::sleep_for(std::chrono::milliseconds(500));
    // setAbsolutePosition(MIDDLE_POS_CAMERA_SERVO_PAN, SERVO_CAMERA_PAN, 1);
    // std::this_thread::sleep_for(std::chrono::milliseconds(500));
    // setAbsolutePosition(MIN_CAM_TILT_POS_VAL, SERVO_CAMERA_TILT, 1);
    // std::this_thread::sleep_for(std::chrono::milliseconds(500));
    // setAbsolutePosition(MAX_CAM_TILT_POS_VAL, SERVO_CAMERA_TILT, 1);
    // std::this_thread::sleep_for(std::chrono::milliseconds(500));
    // setAbsolutePosition(MIDDLE_POS_CAMERA_SERVO_TILT, SERVO_CAMERA_TILT, 1);
    // std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // setAbsolutePosition(MIN_CAM_PAN_POS_VAL, SERVO_CAMERA_PAN, 10);
    // std::this_thread::sleep_for(std::chrono::milliseconds(500));
    // setAbsolutePosition(MAX_CAM_PAN_POS_VAL, SERVO_CAMERA_PAN, 10);
    // std::this_thread::sleep_for(std::chrono::milliseconds(500));
    // setAbsolutePosition(MIDDLE_POS_CAMERA_SERVO_PAN, SERVO_CAMERA_PAN, 10);
    // std::this_thread::sleep_for(std::chrono::milliseconds(500));
    // setAbsolutePosition(MIN_CAM_TILT_POS_VAL, SERVO_CAMERA_TILT, 10);
    // std::this_thread::sleep_for(std::chrono::milliseconds(500));
    // setAbsolutePosition(MAX_CAM_TILT_POS_VAL, SERVO_CAMERA_TILT, 10);
    // std::this_thread::sleep_for(std::chrono::milliseconds(500));
    // setAbsolutePosition(MIDDLE_POS_CAMERA_SERVO_TILT, SERVO_CAMERA_TILT, 10);
    // std::this_thread::sleep_for(std::chrono::milliseconds(500));
}

std::uint8_t ServoController::calculatePosition(uint8_t value, uint8_t max_step) const
{
    uint8_t deadzone = 6;
    uint8_t min_step = 1;
    int d = std::abs(127 - (int)value);
    if (d <= deadzone) return 0;
    float x = float(d - deadzone) / float(127 - deadzone);
    float y = x * x;
    int step = int(std::round(y * max_step));
    // if (step == 0) step = (min_step ? min_step : 1);
    if (step == 0) step = min_step;
    if (step > max_step) step = max_step;
    return (uint8_t)step;
}

OdinServoStep ServoController::getServoPosition(std::uint8_t servo_num)
{
    OdinServoStep servo_data{0, servo_num, m_current_position[servo_num], 0, 0};
    return servo_data;
}
