#ifndef JOYPADHANDLER_H
#define JOYPADHANDLER_H

#include "JoypadData.h"
#include "JoypadShmemHandler.h"

class JoypadHandler
{
public:
    JoypadHandler();
    ~JoypadHandler() = default;

    bool isJoypadConnected();
    void connectAndRun();
    const JoypadDataTypes getJoypadData();
    void setProcessStatus(const bool);

public:
    static constexpr int GENESYS_BYTES_NUM = 27;
    static constexpr int BUFF_SIZE = GENESYS_BYTES_NUM + 1;
    static constexpr int CONTROL_BYTES = 19;
    static constexpr int CONTROL_DATA_BINS = 7;
    static constexpr const char * HIDRAW_PATH = "/dev/hidraw";
    static void signalCallbackHandler(int signum);

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
    static bool m_run_process;
    bool m_joypad_connected;
    int m_joypad_fd;
    char m_buffer[BUFF_SIZE] = {};
    char m_previous_buffer[BUFF_SIZE] = {};

    std::int16_t m_received_bytes;
    std::uint16_t m_connection_timeout_counter;

    JoypadDataTypes m_joypad_data;
    JoypadShmemHandler m_joypad_shmem_handler;
};

#endif // JOYPADHANDLER_H