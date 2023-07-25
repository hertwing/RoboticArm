#ifndef LEDHANDLER_H
#define LEDHANDLER_H

#include <ws2811.h>
#include <cstdint>
#include "DataTypes.h"

class LedHandler
{
public:
    LedHandler();
    ~LedHandler() = default;
    void turnAllLedsOff();
    void setColorToOne(std::uint8_t led_num, ws2811_led_t color);
    void setColorToAll(ws2811_led_t color);
    void setJoypadSelectionColor(std::uint8_t left_analog_servo_num, std::uint8_t right_analog_servo_num);
private:
    ws2811_t m_ledstring;
};

#endif // LEDHANDLER_H