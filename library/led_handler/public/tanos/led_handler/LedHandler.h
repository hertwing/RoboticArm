#ifndef LEDHANDLER_H
#define LEDHANDLER_H

#include "tanos/shmem_wrapper/ShmemHandler.hpp"
#include "DataTypes.h"

#include <ws2811.h>
#include <cstdint>

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
    std::unique_ptr<ShmemWrapper::ShmemHandler<LedState>> m_shmem_handler;
};

#endif // LEDHANDLER_H