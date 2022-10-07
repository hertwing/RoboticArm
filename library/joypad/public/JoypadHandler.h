#ifndef JOYPADHANDLER_H
#define JOYPADHANDLER_H

#include "JoypadData.h"

class JoypadHandler
{
public:
    JoypadHandler();
    ~JoypadHandler() = default;

    bool isJoypadConnected();
    void connectAndRun();
    const JoypadDataTypes getJoypadData();

public:
    static constexpr int GENESYS_BYTES_NUM = 27;
    static constexpr int BUFF_SIZE = GENESYS_BYTES_NUM + 1;
    static const char * HIDRAW_PATH;

private:
    bool checkJoypadConnection(const char *, std::size_t);
    bool createConnection();

    void parseData(const char *);

    void parseRegularButtons(const char *);
    void parseExtraButtons(const char *);
    void parseDPad(const char *);
    void parseLeftStickAxis(const char *);
    void parseRightStickAxis(const char *);

private:
    bool m_joypad_connected;
    int m_joypad_fd;
    char m_buffer[BUFF_SIZE];

    std::int16_t m_received_bytes;
    std::uint16_t m_connection_timeout_counter;

    JoypadDataTypes m_joypad_data;
};

#endif // JOYPADHANDLER_H