#ifndef LEDHANDLERDATATYPES_H
#define LEDHANDLERDATATYPES_H

#include <ws2811.h>
#include <cstdint>

namespace led_handler {

static constexpr std::uint32_t TARGET_FREQ = 800000; // WS2812B LED STRIP COMMUNICATION FREQ IN HZ
static constexpr std::uint8_t GPIO_PIN = 18; // PWM PIN ON RPI BOARD TO DRIVE LED STRIP
static constexpr std::uint8_t DMA = 5;
static constexpr std::uint32_t STRIP_TYPE = WS2812_STRIP;
static constexpr std::uint8_t LED_COUNT = 6;

static constexpr ws2811_led_t LED_COLOR_NONE = 0x00000000;
static constexpr ws2811_led_t LED_COLOR_RED = 0x00200000;
static constexpr ws2811_led_t LED_COLOR_ORANGE = 0x00201000;
static constexpr ws2811_led_t LED_COLOR_YELLOW = 0x00202000;
static constexpr ws2811_led_t LED_COLOR_GREEN = 0x00002000;
static constexpr ws2811_led_t LED_COLOR_LIGHTBLUE = 0x00002020;
static constexpr ws2811_led_t LED_COLOR_BLUE = 0x00000020;
static constexpr ws2811_led_t LED_COLOR_PURPLE = 0x00100010;
static constexpr ws2811_led_t LED_COLOR_PINK = 0x00200010;

enum JointLed {
    BASE_ROTOR_LED,
    JOINT_ONE_LED,
    JOINT_TWO_LED,
    JOINT_THREE_LED,
    GRIPPER_BASE_LED,
    GRIPPER_LED,
};

} // led_handler

#endif // LEDHANDLERDATATYPES_H