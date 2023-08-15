#ifndef LEDHANDLER_H
#define LEDHANDLER_H

#include "odin/shmem_wrapper/ShmemHandler.hpp"
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
    void updateLedColors();
    static void signalCallbackHandler(int signum);

private:
    ws2811_t m_ledstring;
    ws2811_led_t m_led_color_status[led_handler::LED_COUNT];
    std::unique_ptr<odin::shmem_wrapper::ShmemHandler<ws2811_led_t>> m_shmem_handler;
    bool m_is_color_changed;
    static bool m_run_process;
};

#endif // LEDHANDLER_H