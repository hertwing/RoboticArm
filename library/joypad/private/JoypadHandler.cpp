#include "JoypadHandler.h"
#include "JoypadShmemHandler.h"

#include <bitset>
#include <chrono>
#include <fcntl.h>
#include <experimental/filesystem>
#include <iostream>
#include <string>
#include <string.h>
#include <thread>
#include <unistd.h>

namespace fs = std::experimental::filesystem;

bool JoypadHandler::m_run_process = true;

JoypadHandler::JoypadHandler()
{
    m_joypad_connected = false;
    m_joypad_fd = -1;
}

const JoypadDataTypes JoypadHandler::getJoypadData()
{
    return m_joypad_data;
}

bool JoypadHandler::isJoypadConnected()
{
    return m_joypad_connected;
}

bool JoypadHandler::checkJoypadConnection(const char * buffer, std::size_t buff_size)
{
    std::cout << "Checking connection" << std::endl;
    if (buff_size == GENESYS_BYTES_NUM)
    {
        if (buffer[20] == 2 && buffer[22] == 2 && buffer[24] == 2 && buffer[26] == 2 &&
            buffer[23] == 0 && buffer[25] == 0 &&
            buffer[19] == buffer[21])
        {
            return true;
        }
    }
    return false;
}

bool JoypadHandler::createConnection()
{
    std::cout << "Trying to obtain joypad hidraw fd." << std::endl;
    for (const auto & fd : fs::directory_iterator("/dev/"))
    {
        if (fd.path().string().find(HIDRAW_PATH) == std::string::npos)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            break;
        }
        if ((m_joypad_fd = open(fd.path().c_str(), O_RDONLY | O_NONBLOCK)) > -1)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            if ((m_received_bytes = read(m_joypad_fd, m_buffer, BUFF_SIZE)) > 0)
            {
                std::cout << "Reading from hidraw descriptor." << std::endl;
                if (checkJoypadConnection(m_buffer, m_received_bytes))
                {
                    std::cout << "Joypad connected." << std::endl;
                    m_joypad_connected = true;
                    break;
                }
                else
                {
                    std::cerr << "Joypad not found." << std::endl;
                    m_joypad_connected = false;
                    close(m_joypad_fd);
                    break;
                }
            }
        }
        else
        {
            std::cerr << "Coulnd't open file descriptor." << std::endl;
            m_joypad_connected = false;
            close(m_joypad_fd);
        }
    }
    return m_joypad_connected;
}

void JoypadHandler::connectAndRun()
{
    createConnection();
    m_connection_timeout_counter = 0;
    while (m_joypad_connected && m_run_process)
    {
        while ((m_received_bytes = read(m_joypad_fd, m_buffer, BUFF_SIZE)) > 0)
        {
            m_connection_timeout_counter = 0;
            // Update joypad data only when control bytes changes their values.
            for (int i = 0; i < CONTROL_BYTES; ++i)
            {
                if (m_previous_buffer[i] != m_buffer[i])
                {
                    parseData(m_buffer);
                    m_joypad_shmem_handler.writeJoypadData(m_joypad_data.createJoypadData());
                    for (int i = 0; i < BUFF_SIZE; ++i)
                    {
                        m_previous_buffer[i] = m_buffer[i];
                    }
                    break;
                }
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        ++m_connection_timeout_counter;
        if (m_connection_timeout_counter == 2000)
        {
            std::cerr << "Connection with Joypad lost!" << std::endl;
            m_joypad_connected = false;
            m_connection_timeout_counter = 0;
            close(m_joypad_fd);
            m_joypad_fd = -1;
        }
    }
}

void JoypadHandler::parseData(const char * data)
{
    parseRegularButtons(data);
    parseExtraButtons(data);
    parseDPad(data);
    parseLeftStickAxis(data);
    parseRightStickAxis(data);
}

void JoypadHandler::parseRegularButtons(const char * data)
{
    m_joypad_data.rightTrigger = std::bitset<8>(data[0])[7] && data[18];
    m_joypad_data.leftTrigger  = std::bitset<8>(data[0])[6] && data[17];
    m_joypad_data.rightBumper  = std::bitset<8>(data[0])[5] && data[16];
    m_joypad_data.leftBumper   = std::bitset<8>(data[0])[4] && data[15];
    m_joypad_data.buttonX      = std::bitset<8>(data[0])[3] && data[14];
    m_joypad_data.buttonA      = std::bitset<8>(data[0])[2] && data[13];
    m_joypad_data.buttonB      = std::bitset<8>(data[0])[1] && data[12];
    m_joypad_data.buttonY      = std::bitset<8>(data[0])[0] && data[11];
}

void JoypadHandler::parseExtraButtons(const char * data)
{
    m_joypad_data.buttonRightStick = std::bitset<8>(data[1])[3];
    m_joypad_data.buttonLeftStick = std::bitset<8>(data[1])[2];
    m_joypad_data.buttonStart = std::bitset<8>(data[1])[1];
    m_joypad_data.buttonSelect = std::bitset<8>(data[1])[0];
}

void JoypadHandler::parseDPad(const char * data)
{
    m_joypad_data.dPadUp = data[2] == 0 && static_cast<unsigned char>(data[9]) > 0;
    m_joypad_data.dPadUpRight = data[2] == 1 && static_cast<unsigned char>(data[9]) > 0 && static_cast<unsigned char>(data[7]) > 0;
    m_joypad_data.dPadRight = data[2] == 2 && static_cast<unsigned char>(data[7]) > 0;
    m_joypad_data.dPadDownRight = data[2] == 3 && static_cast<unsigned char>(data[7]) > 0 && static_cast<unsigned char>(data[10]) > 0;
    m_joypad_data.dPadDown = data[2] == 4 && static_cast<unsigned char>(data[10]) > 0;
    m_joypad_data.dPadDownLeft = data[2] == 5 && static_cast<unsigned char>(data[10]) > 0 && static_cast<unsigned char>(data[8]) > 0;
    m_joypad_data.dPadLeft = data[2] == 6 && static_cast<unsigned char>(data[8]) > 0;
    m_joypad_data.dPadUpLeft = data[2] == 7 && static_cast<unsigned char>(data[8]) > 0 && static_cast<unsigned char>(data[9]) > 0;
}

void JoypadHandler::parseLeftStickAxis(const char * data)
{
    m_joypad_data.leftStickX = data[3];
    m_joypad_data.leftStickY = data[4];
}

void JoypadHandler::parseRightStickAxis(const char * data)
{
    m_joypad_data.rightStickX = data[5];
    m_joypad_data.rightStickY = data[6];
}

void JoypadHandler::signalCallbackHandler(int signum)
{
    std::cout << "JoypadHandler received signal: " << signum << std::endl;
    m_run_process = false;
}