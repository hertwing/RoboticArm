#include "tanos/led_handler/DataTypes.h"
#include "tanos/led_handler/LedHandler.h"
#include "tanos/shmem_wrapper/DataTypes.h"

LedHandler::LedHandler()
{
    m_ledstring =
    {
        .freq = TARGET_FREQ,
        .dmanum = DMA,
        .channel =
        {
            [0] =
            {
                .gpionum = GPIO_PIN,
                .invert = 0,
                .count = LED_COUNT,
                .strip_type = STRIP_TYPE,
                .brightness = 255,
            },
            [1] =
            {
                .gpionum = 0,
                .invert = 0,
                .count = 0,
                .brightness = 0,
            },
        },
    };
    ws2811_init(&m_ledstring);
    turnAllLedsOff();
    m_shmem_handler = std::make_unique<shmem_wrapper::ShmemHandler<LedState>>(
        shmem_wrapper::DataTypes::, CONTROL_DATA_BINS, std::to_string(getpid()).c_str(), true, shmem_wrapper::DataTypes::JOYPAD_SEM_NAME);
}

void LedHandler::turnAllLedsOff()
{
    for (int i = 0; i < LED_COUNT; ++i)
    {
        m_ledstring.channel[0].leds[i] = LED_COLOR_NONE;
    }
    ws2811_render(&m_ledstring);
}

void LedHandler::setColorToOne(std::uint8_t led_num, ws2811_led_t color)
{
    for (int i = 0; i < LED_COUNT; ++i)
    {
        m_ledstring.channel[0].leds[i] = LED_COLOR_NONE;
    }
    m_ledstring.channel[0].leds[led_num] = color;
    ws2811_render(&m_ledstring);
}

void LedHandler::setColorToAll(ws2811_led_t color)
{
    for (int i = 0; i < LED_COUNT; ++i)
    {
        m_ledstring.channel[0].leds[i] = color;
    }
    ws2811_render(&m_ledstring);
}

void LedHandler::setJoypadSelectionColor(std::uint8_t left_analog_servo_num, std::uint8_t right_analog_servo_num)
{
    for (int i = 0; i < LED_COUNT; ++i)
    {
        m_ledstring.channel[0].leds[i] = LED_COLOR_NONE;
    }
    m_ledstring.channel[0].leds[left_analog_servo_num] = LED_COLOR_BLUE;
    m_ledstring.channel[0].leds[right_analog_servo_num] = LED_COLOR_GREEN;
    ws2811_render(&m_ledstring);
}