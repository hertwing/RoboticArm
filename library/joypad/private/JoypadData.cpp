#include "JoypadData.h"

#include <iostream>

namespace
{
    constexpr std::uint8_t setBit(bool value, unsigned bit)
    {
        return value ? static_cast<std::uint8_t>(1u << bit) : 0u;
    }

    constexpr bool getBit(std::uint8_t byte, unsigned bit)
    {
        return (byte & static_cast<std::uint8_t>(1u << bit)) != 0;
    }
}

JoypadData JoypadState::createJoypadData() const
{
    JoypadData joypadData{};

    joypadData.data[0] =
        setBit(leftTrigger,  0) |
        setBit(rightTrigger, 1) |
        setBit(leftBumper,   2) |
        setBit(rightBumper,  3) |
        setBit(buttonA,      4) |
        setBit(buttonB,      5) |
        setBit(buttonX,      6) |
        setBit(buttonY,      7);

    joypadData.data[1] =
        setBit(dPadUp,        0) |
        setBit(dPadUpLeft,    1) |
        setBit(dPadUpRight,   2) |
        setBit(dPadDown,      3) |
        setBit(dPadDownLeft,  4) |
        setBit(dPadDownRight, 5) |
        setBit(dPadLeft,      6) |
        setBit(dPadRight,     7);

    joypadData.data[2] =
        setBit(buttonStart,      0) |
        setBit(buttonSelect,     1) |
        setBit(buttonLeftStick,  2) |
        setBit(buttonRightStick, 3);

    joypadData.data[3] = leftStickX;
    joypadData.data[4] = leftStickY;
    joypadData.data[5] = rightStickX;
    joypadData.data[6] = rightStickY;

    return joypadData;
}

JoypadState JoypadState::fromJoypadData(const JoypadData& data)
{
    JoypadState state;

    state.leftTrigger  = getBit(data.data[0], 0);
    state.rightTrigger = getBit(data.data[0], 1);
    state.leftBumper   = getBit(data.data[0], 2);
    state.rightBumper  = getBit(data.data[0], 3);
    state.buttonA      = getBit(data.data[0], 4);
    state.buttonB      = getBit(data.data[0], 5);
    state.buttonX      = getBit(data.data[0], 6);
    state.buttonY      = getBit(data.data[0], 7);

    state.dPadUp        = getBit(data.data[1], 0);
    state.dPadUpLeft    = getBit(data.data[1], 1);
    state.dPadUpRight   = getBit(data.data[1], 2);
    state.dPadDown      = getBit(data.data[1], 3);
    state.dPadDownLeft  = getBit(data.data[1], 4);
    state.dPadDownRight = getBit(data.data[1], 5);
    state.dPadLeft      = getBit(data.data[1], 6);
    state.dPadRight     = getBit(data.data[1], 7);

    state.buttonStart      = getBit(data.data[2], 0);
    state.buttonSelect     = getBit(data.data[2], 1);
    state.buttonLeftStick  = getBit(data.data[2], 2);
    state.buttonRightStick = getBit(data.data[2], 3);

    state.leftStickX  = data.data[3];
    state.leftStickY  = data.data[4];
    state.rightStickX = data.data[5];
    state.rightStickY = data.data[6];

    return state;
}

void JoypadState::printJoypadData()
{
    std::cout << leftTrigger << std::endl;
    std::cout << rightTrigger << std::endl;
    std::cout << leftBumper << std::endl;
    std::cout << rightBumper << std::endl;
    std::cout << +leftStickX << std::endl;
    std::cout << +leftStickY << std::endl;
    std::cout << +rightStickX << std::endl;
    std::cout << +rightStickY << std::endl;
}