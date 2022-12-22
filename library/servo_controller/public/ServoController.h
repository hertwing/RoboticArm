#ifndef SERVOCONTROLLER_H
#define SERVOCONTROLLER_H

#include <cstdint>

static constexpr std::uint16_t PIN_BASE = 300;
static constexpr std::uint16_t MAX_PWM = 4096;
static constexpr std::uint8_t HERTZ = 50;
static constexpr std::uint8_t SERVO_NUM = 6;
static constexpr std::uint16_t MIDDLE_POS_VAL = 1500;
static constexpr std::uint16_t MIN_POS_VAL = 500;
static constexpr std::uint16_t MAX_POS_VAL = 2500;

static constexpr std::uint16_t SERVO_1_MIN_POS_VAL = 1350;
static constexpr std::uint16_t SERVO_1_MAX_POS_VAL = 2300;

static constexpr std::uint16_t STARTUP_POS_SERVO_0 = 1500;
static constexpr std::uint16_t STARTUP_POS_SERVO_1 = 2000;
static constexpr std::uint16_t STARTUP_POS_SERVO_2 = 650;
static constexpr std::uint16_t STARTUP_POS_SERVO_3 = 2400;
static constexpr std::uint16_t STARTUP_POS_SERVO_4 = 1500;
static constexpr std::uint16_t STARTUP_POS_SERVO_5 = 1500;

const std::uint16_t STARTUP_POSITIONS[SERVO_NUM] = 
{
    STARTUP_POS_SERVO_0,
    STARTUP_POS_SERVO_1,
    STARTUP_POS_SERVO_2,
    STARTUP_POS_SERVO_3,
    STARTUP_POS_SERVO_4,
    STARTUP_POS_SERVO_5
};

static constexpr std::uint16_t MIDDLE_POS_SERVO_0 = 1500;
static constexpr std::uint16_t MIDDLE_POS_SERVO_1 = 1575;
static constexpr std::uint16_t MIDDLE_POS_SERVO_2 = 1630;
static constexpr std::uint16_t MIDDLE_POS_SERVO_3 = 1480;
static constexpr std::uint16_t MIDDLE_POS_SERVO_4 = 1500;
static constexpr std::uint16_t MIDDLE_POS_SERVO_5 = 1500;

const std::uint16_t MIDDLE_POSITIONS[SERVO_NUM] = 
{
    MIDDLE_POS_SERVO_0,
    MIDDLE_POS_SERVO_1,
    MIDDLE_POS_SERVO_2,
    MIDDLE_POS_SERVO_3,
    MIDDLE_POS_SERVO_4,
    MIDDLE_POS_SERVO_5
};

class ServoController
{
public:
    ServoController();
    ~ServoController() = default;

    void setAbsolutePosition(int position, int servo_num, int step);
    void moveLeft(int servo_num, std::uint8_t value);
    void moveRight(int servo_num, std::uint8_t value);

private:
    int calcTicks(float impulseMs, int hertz);

    void setStartupPosition();
    std::uint8_t calculatePosition(std::uint8_t value, std::uint8_t max_step) const;
private:
    int m_current_position[SERVO_NUM];
};

#endif // SERVOCONTROLLER_H