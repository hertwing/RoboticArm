#ifndef JOYPADDATA_H
#define JOYPADDATA_H

#include <array>
#include <cstdint>

struct JoypadData
{
    static constexpr std::size_t Size = 7;
    std::array<std::uint8_t, Size> data{};
};

struct JoypadState
{
    bool leftBumper   = false;
    bool rightBumper  = false;
    bool leftTrigger  = false;
    bool rightTrigger = false;

    bool buttonA = false;
    bool buttonB = false;
    bool buttonX = false;
    bool buttonY = false;

    bool dPadUp        = false;
    bool dPadUpRight   = false;
    bool dPadRight     = false;
    bool dPadDownRight = false;
    bool dPadDown      = false;
    bool dPadDownLeft  = false;
    bool dPadLeft      = false;
    bool dPadUpLeft    = false;

    bool buttonSelect = false;
    bool buttonStart  = false;

    bool buttonLeftStick  = false;
    bool buttonRightStick = false;

    std::uint8_t leftStickX;
    std::uint8_t leftStickY;
    std::uint8_t rightStickX;
    std::uint8_t rightStickY;

    JoypadData createJoypadData() const;
    static JoypadState fromJoypadData(const JoypadData & data);
    // void parseJoypadData(const JoypadData & data);
    void printJoypadData();
};

#endif // JOYPADDATA_H