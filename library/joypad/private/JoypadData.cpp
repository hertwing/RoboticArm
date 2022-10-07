#include "JoypadData.h"

const JoypadData JoypadDataTypes::createJoypadData()
{
    JoypadData joypad_data;
    joypad_data.data[0] = leftTrigger | (rightTrigger << 1) | (leftBumper << 2) | (rightBumper << 3) | (buttonA << 4) | (buttonB << 5) | (buttonX << 6) | (buttonY << 7);
    joypad_data.data[1] = dPadUp | (dPadUpLeft << 1) | (dPadUpRight << 2) | (dPadDown << 3) | (dPadDownLeft << 4) | (dPadDownRight << 5) | (dPadLeft << 6) | (dPadRight << 7);
    joypad_data.data[2] = buttonStart | (buttonSelect << 1) | (buttonLeftStick << 2) | (buttonRightStick << 3);
    joypad_data.data[3] = leftStickX;
    joypad_data.data[4] = leftStickY;
    joypad_data.data[5] = rightStickX;
    joypad_data.data[6] = rightStickY;

    return joypad_data;
}

void JoypadDataTypes::parseJoypadData(const JoypadData & joypad_data)
{
    leftTrigger = joypad_data.data[0] & 1;
    rightTrigger = (joypad_data.data[0] & (1 << 1)) >> 1;
    leftBumper = (joypad_data.data[0] & (1 << 2)) >> 2;
    rightBumper = (joypad_data.data[0] & (1 << 3)) >> 3;
    buttonA = (joypad_data.data[0] & (1 << 4)) >> 4;
    buttonB = (joypad_data.data[0] & (1 << 5)) >> 5;
    buttonX = (joypad_data.data[0] & (1 << 6)) >> 6;
    buttonY = (joypad_data.data[0] & (1 << 7)) >> 7;

    dPadUp = joypad_data.data[1] & 1;
    dPadUpLeft = (joypad_data.data[1] & (1 << 1)) >> 1;
    dPadUpRight = (joypad_data.data[1] & (1 << 2)) >> 2;
    dPadDown = (joypad_data.data[1] & (1 << 3)) >> 3;
    dPadDownLeft = (joypad_data.data[1] & (1 << 4)) >> 4;
    dPadDownRight = (joypad_data.data[1] & (1 << 5)) >> 5;
    dPadLeft = (joypad_data.data[1] & (1 << 6)) >> 6;
    dPadRight = (joypad_data.data[1] & (1 << 7)) >> 7;

    buttonStart = joypad_data.data[2] & 1;
    buttonSelect = (joypad_data.data[2] & (1 << 1)) >> 1;
    buttonLeftStick = (joypad_data.data[2] & (1 << 2)) >> 2;
    buttonRightStick = (joypad_data.data[2] & (1 << 3)) >> 3;

    leftStickX = joypad_data.data[3];
    leftStickY = joypad_data.data[4];
    rightStickX = joypad_data.data[5];
    leftStickY = joypad_data.data[6];
}