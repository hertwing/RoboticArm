#ifndef JOYPADDATA_H
#define JOYPADDATA_H

#include <cstdint>

struct JoypadData
{
    std::uint8_t data[7];
};

struct JoypadDataTypes
{
    std::uint8_t leftBumper   : 1;
    std::uint8_t rightBumper  : 1;
    std::uint8_t leftTrigger  : 1;
    std::uint8_t rightTrigger : 1;

    std::uint8_t buttonA : 1;
    std::uint8_t buttonB : 1;
    std::uint8_t buttonX : 1;
    std::uint8_t buttonY : 1;

    std::uint8_t dPadUp        : 1;
    std::uint8_t dPadUpRight   : 1;
    std::uint8_t dPadRight     : 1;
    std::uint8_t dPadDownRight : 1;
    std::uint8_t dPadDown      : 1;
    std::uint8_t dPadDownLeft  : 1;
    std::uint8_t dPadLeft      : 1;
    std::uint8_t dPadUpLeft    : 1;

    std::uint8_t buttonStart  : 1;
    std::uint8_t buttonSelect : 1;

    std::uint8_t buttonLeftStick  : 1;
    std::uint8_t buttonRightStick : 1;

    std::uint8_t leftStickX;
    std::uint8_t leftStickY;
    std::uint8_t rightStickX;
    std::uint8_t rightStickY;

    const JoypadData createJoypadData();
    void parseJoypadData(const JoypadData & data);
    void printJoypadData();
};

#endif // JOYPADDATA_H