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
    cameraMoveTest();
}

ServoController::~ServoController() {
    if (m_i2c_fd >= 0)
    {
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
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
}

// void ServoController::setAbsolutePosition(uint16_t target_pos, uint8_t servo_num, uint8_t step)
// {
//     if (m_current_position[servo_num] == target_pos) return;
//     if (servo_num >= SERVO_NUM)
//     {
//         return;
//     }
//     if (step == 0) step = 1;
//     if (step > 10) step = 10;

//     std::uint16_t min_val = MIN_POS_VAL;
//     std::uint16_t max_val = MAX_POS_VAL;

//     if (servo_num == SERVO_ARM_1)
//     {
//         min_val = SERVO_1_MIN_POS_VAL;
//         max_val = SERVO_1_MAX_POS_VAL;
//     }
//     else if (servo_num == SERVO_GRIPPER)
//     {
//         min_val = GRIPPER_MIN_POS_VAL;
//         max_val = GRIPPER_MAX_POS_VAL;
//     }
//     else if (servo_num == SERVO_CAMERA_1)
//     {
//         min_val = MIN_CAM_1_POS_VAL;
//         max_val = MAX_CAM_1_POS_VAL;
//     }

//     std::int32_t current_pos = static_cast<std::int32_t>(m_current_position[servo_num]);

//     target_pos = std::clamp(target_pos, min_val, max_val);

//     std::cout << "Moving servo number: " << +servo_num
//               << " to position: " << target_pos
//               << ". Step: " << +step << "." << std::endl;

//     while (current_pos != target_pos)
//     {
//         if (current_pos < target_pos) 
//         {
//             current_pos += step;
//             if (current_pos > target_pos) current_pos = target_pos;
//         }
//         else 
//         {
//             current_pos -= step;
//             if (current_pos < target_pos) current_pos = target_pos;
//         }

//         float millis = static_cast<float>(current_pos) / 1000.0f;
//         int tick = calcTicks(millis, HERTZ);
//         setTick(m_i2c_fd, servo_num, static_cast<uint16_t>(tick));
//         m_current_position[servo_num] = static_cast<std::uint16_t>(current_pos);

//         if (step < 3)
//         {
//             std::this_thread::sleep_for(std::chrono::milliseconds(20));
//         } 
//         else if (step < 6 && step >= 3)
//         {
//             std::this_thread::sleep_for(std::chrono::milliseconds(7));
//         } 
//         else 
//         {
//             std::this_thread::sleep_for(std::chrono::milliseconds(1));
//         }
//     }
// }

void ServoController::setAbsolutePosition(uint16_t target_pos, uint8_t servo_num, uint8_t step)
{
    if (m_current_position[servo_num] == target_pos) return;
    if (servo_num >= SERVO_NUM)
    {
        return;
    }
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
    else if (servo_num == SERVO_CAMERA_1)
    {
        min_val = MIN_CAM_1_POS_VAL;
        max_val = MAX_CAM_1_POS_VAL;
    }

    std::int32_t current_pos = static_cast<std::int32_t>(m_current_position[servo_num]);

    target_pos = std::clamp(target_pos, min_val, max_val);

    std::cout << "Moving servo number: " << +servo_num
            << " to position: " << target_pos
            << ". Step: " << +step << "." << std::endl;
    // -------- wariant 1: stały rytm 50 Hz + prędkość z dt --------
    using clock = std::chrono::steady_clock;

    // Speed and "step per frame" (20 ms @ 50 Hz)
    const float v_us_s = SPEED_PER_STEP * static_cast<float>(step);
    const int   per_frame_us = std::max(1, static_cast<int>(std::ceil(v_us_s * 0.020f)));
    const int   abs_dist_us  = std::abs(static_cast<int>(target_pos) - static_cast<int>(current_pos));

    // FAST-PATH:
    if (abs_dist_us <= std::max<int>(EPS_US, per_frame_us)) {
        current_pos = target_pos;
        float ms = static_cast<float>(current_pos) / 1000.0f;
        int tick = calcTicks(ms, HERTZ);
        if (static_cast<uint16_t>(current_pos) != m_current_position[servo_num]) {
            setTick(m_i2c_fd, servo_num, static_cast<uint16_t>(tick));
            m_current_position[servo_num] = static_cast<std::uint16_t>(current_pos);
        }
        return;
    }
    auto last   = clock::now();
    auto next_t = last + FRAME;
    uint16_t last_written = m_current_position[servo_num];

    while (current_pos != static_cast<std::int32_t>(target_pos)) {
        // time since previous iteration
        auto now = clock::now();
        float dt = std::chrono::duration<float>(now - last).count();
        if (dt <= 0.0f) dt = 0.02f; // fallback ~20 ms
        last = now;

        // distance and direction
        int dist = static_cast<int>(target_pos) - static_cast<int>(current_pos);
        int sign = (dist > 0) ? 1 : -1;
        int abs_dist = std::abs(dist);

        int step_us = static_cast<int>(std::ceil(v_us_s * dt));
        if (step_us < 1) step_us = 1;
        if (step_us > abs_dist) step_us = abs_dist;

        current_pos += sign * step_us;

        uint16_t new_us = static_cast<uint16_t>(current_pos);
        if (static_cast<int>(std::abs(static_cast<int>(new_us) - static_cast<int>(last_written))) >= MIN_WRITE_DELTA_US) {
            float ms = static_cast<float>(new_us) / 1000.0f;
            int tick = calcTicks(ms, HERTZ);
            setTick(m_i2c_fd, servo_num, static_cast<uint16_t>(tick));
            m_current_position[servo_num] = new_us;
            last_written = new_us;
        }

        // keep 50 Hz
        next_t += FRAME;
        std::this_thread::sleep_until(next_t);
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

void ServoController::cameraMoveLeft(std::uint8_t servo_num, std::uint8_t value)
{
    if (servo_num != SERVO_CAMERA_1) return;
    setAbsolutePosition(m_current_position[servo_num] + value, servo_num, value);
}

void ServoController::cameraMoveRight(std::uint8_t servo_num, std::uint8_t value)
{
    if (servo_num != SERVO_CAMERA_1) return;
    setAbsolutePosition(m_current_position[servo_num] - value, servo_num, value);
}

void ServoController::cameraMoveUp(std::uint8_t servo_num, std::uint8_t value)
{
    if (servo_num != SERVO_CAMERA_2) return;
    setAbsolutePosition(m_current_position[servo_num] + value, servo_num, value);
}

void ServoController::cameraMoveDown(std::uint8_t servo_num, std::uint8_t value)
{
    if (servo_num != SERVO_CAMERA_2) return;
    setAbsolutePosition(m_current_position[servo_num] - value, servo_num, value);
}

void ServoController::cameraMoveTest()
{
    // while(true)
    // {
    setAbsolutePosition(MIN_POS_VAL, 0, 5);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    setAbsolutePosition(MAX_POS_VAL, 0, 5);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    setAbsolutePosition(MIDDLE_POS_SERVO_0, 0, 5);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    // setAbsolutePosition(MIN_CAM_1_POS_VAL, 6, 5);
    // std::this_thread::sleep_for(std::chrono::milliseconds(500));
    // setAbsolutePosition(MAX_CAM_1_POS_VAL, 6, 5);
    // std::this_thread::sleep_for(std::chrono::milliseconds(500));
    // setAbsolutePosition(MIDDLE_POS_CAMERA_SERVO_1, 6, 5);
    // std::this_thread::sleep_for(std::chrono::milliseconds(500));
    // setAbsolutePosition(MIN_POS_VAL, 7, 5);
    // std::this_thread::sleep_for(std::chrono::milliseconds(500));
    // setAbsolutePosition(MAX_POS_VAL, 7, 5);
    // std::this_thread::sleep_for(std::chrono::milliseconds(500));
    // setAbsolutePosition(MIDDLE_POS_CAMERA_SERVO_2, 7, 5);
    // std::this_thread::sleep_for(std::chrono::milliseconds(500));
    // }
    // std::cout << "Camera test" << std::endl;
    // while(true)
    // {
    //     while(m_current_position[CAMERA_SERVO_1] < MAX_CAM_POS_VAL)
    //     {
    //         std::cout << m_current_position[CAMERA_SERVO_1] << std::endl;
    //         cameraMoveLeft(CAMERA_SERVO_1, 4);
    //     }
    //     std::this_thread::sleep_for(std::chrono::milliseconds(250));
    //     while(m_current_position[CAMERA_SERVO_1] > MIN_CAM_POS_VAL)
    //     {
    //         std::cout << m_current_position[CAMERA_SERVO_1] << std::endl;
    //         cameraMoveRight(CAMERA_SERVO_1, 4);
    //     }
    //     std::this_thread::sleep_for(std::chrono::milliseconds(250));
    //     while(m_current_position[CAMERA_SERVO_1] < STARTUP_POS_CAMERA_SERVO_1)
    //     {
    //         std::cout << m_current_position[CAMERA_SERVO_1] << std::endl;
    //         cameraMoveLeft(CAMERA_SERVO_1, 4);
    //     }
    //     std::this_thread::sleep_for(std::chrono::milliseconds(250));
    //     // while(m_current_position[CAMERA_SERVO_2] < MAX_POS_VAL)
    //     // {
    //     //     std::cout << m_current_position[CAMERA_SERVO_2] << std::endl;
    //     //     cameraMoveUp(CAMERA_SERVO_2, 4);
    //     // }
    //     // std::this_thread::sleep_for(std::chrono::milliseconds(250));
    //     // while(m_current_position[CAMERA_SERVO_2] > MIN_POS_VAL)
    //     // {
    //     //     std::cout << m_current_position[CAMERA_SERVO_2] << std::endl;
    //     //     cameraMoveDown(CAMERA_SERVO_2, 4);
    //     // }
    //     // std::this_thread::sleep_for(std::chrono::milliseconds(250));
    //     // while(m_current_position[CAMERA_SERVO_2] < STARTUP_POS_CAMERA_SERVO_2)
    //     // {
    //     //     std::cout << m_current_position[CAMERA_SERVO_2] << std::endl;
    //     //     cameraMoveUp(CAMERA_SERVO_2, 4);
    //     // }
    //     std::this_thread::sleep_for(std::chrono::milliseconds(250));
    // }
}

std::uint8_t ServoController::calculatePosition(uint8_t value, uint8_t max_step) const
{
    return (std::abs(127 - value) * max_step + 63) / 127;
}