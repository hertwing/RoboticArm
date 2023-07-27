#include "odin/led_handler/DataTypes.h"
#include "odin/led_handler/LedHandler.h"
#include "odin/shmem_wrapper/DataTypes.h"

#include <iostream>
#include <chrono>
#include <thread>

LedHandler::LedHandler()
{
    m_ledstring =
    {
        .freq = led_handler::TARGET_FREQ,
        .dmanum = led_handler::DMA,
        .channel =
        {
            [0] =
            {
                .gpionum = led_handler::GPIO_PIN,
                .invert = 0,
                .count = led_handler::LED_COUNT,
                .strip_type = led_handler::STRIP_TYPE,
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
    m_shmem_handler = std::make_unique<shmem_wrapper::ShmemHandler<ws2811_led_t>>(
        shmem_wrapper::DataTypes::LED_SHMEM_NAME, led_handler::LED_COUNT, true);

    // Fill leds shmem with none values
    for (int i = 0; i < led_handler::LED_COUNT; ++i)
    {
        m_led_color_status[i] = led_handler::LED_COLOR_NONE;
    }
    m_shmem_handler->shmemWrite(m_led_color_status);
    m_is_color_changed = false;
}

void LedHandler::turnAllLedsOff()
{
    for (int i = 0; i < led_handler::LED_COUNT; ++i)
    {
        m_ledstring.channel[0].leds[i] = led_handler::LED_COLOR_NONE;
    }
    ws2811_render(&m_ledstring);
}

void LedHandler::setColorToOne(std::uint8_t led_num, ws2811_led_t color)
{
    for (int i = 0; i < led_handler::LED_COUNT; ++i)
    {
        m_ledstring.channel[0].leds[i] = led_handler::LED_COLOR_NONE;
    }
    m_ledstring.channel[0].leds[led_num] = color;
    ws2811_render(&m_ledstring);
}

void LedHandler::setColorToAll(ws2811_led_t color)
{
    for (int i = 0; i < led_handler::LED_COUNT; ++i)
    {
        m_ledstring.channel[0].leds[i] = color;
    }
    ws2811_render(&m_ledstring);
}

void LedHandler::updateLedColors()
{
    if (m_shmem_handler->shmemRead(m_led_color_status))
    {
        // Check if colors changed
        // std::cout << "Updating Led colors" << std::endl;
        for (int i = 0; i < led_handler::LED_COUNT; ++i)
        {
            if (m_ledstring.channel[0].leds[i] != m_led_color_status[i])
            {
                m_is_color_changed = true;
            }
        }
        if (m_is_color_changed)
        {

            std::cout << "Color changed" << std::endl;
            for (int i = 0; i < led_handler::LED_COUNT; ++i)
            {
                m_ledstring.channel[0].leds[i] = m_led_color_status[i];
            }
            ws2811_render(&m_ledstring);
        }
        m_is_color_changed = false;
    }
    else
    {
        std::cout << "Couldn't read from shmem." << std::endl;
    }
}
