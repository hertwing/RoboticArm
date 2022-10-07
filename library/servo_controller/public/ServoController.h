#ifndef SERVOCONTROLLER_H
#define SERVOCONTROLLER_H

class ServoController
{
public:
    ServoController();
    ~ServoController() = default;

    void setAbsolutePosition(int position, int servo_num);

public:
    static constexpr int PIN_BASE = 300;
    static constexpr int MAX_PWM = 4096;
    static constexpr int HERTZ = 50;
    static constexpr int SERVO_NUM = 2;
    static constexpr int MIDDLE_POS_VAL = 1500;
    static constexpr int MIN_POS_VAL = 500;
    static constexpr int MAX_POS_VAL = 2500;

private:
    int calcTicks(float impulseMs, int hertz);

    void setStartupPosition();

private:
    int m_current_position[SERVO_NUM];
};

#endif // SERVOCONTROLLER_H