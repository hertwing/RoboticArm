#include <cstdint>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include <unistd.h>
#include <thread>
#include <chrono>
#include <cmath>

// ===== PCA9685 register map =====
namespace {
constexpr uint8_t PCA9685_ADDR   = 0x40;
constexpr uint8_t MODE1          = 0x00;
constexpr uint8_t MODE2          = 0x01;
constexpr uint8_t SUBADR1        = 0x02;
constexpr uint8_t SUBADR2        = 0x03;
constexpr uint8_t SUBADR3        = 0x04;
constexpr uint8_t PRESCALE       = 0xFE;
constexpr uint8_t LED0_ON_L      = 0x06; // channel 0 start
constexpr uint8_t ALL_LED_ON_L   = 0xFA;
constexpr uint8_t ALL_LED_ON_H   = 0xFB;
constexpr uint8_t ALL_LED_OFF_L  = 0xFC;
constexpr uint8_t ALL_LED_OFF_H  = 0xFD;

// MODE1 bits
constexpr uint8_t RESTART = 0x80;
constexpr uint8_t SLEEP   = 0x10;
constexpr uint8_t AI      = 0x20; // Auto-Increment

// MODE2 bits
constexpr uint8_t OUTDRV  = 0x04; // totem-pole
constexpr uint8_t OCH     = 0x08; // update on ACK

// PCA9685 clock
constexpr double OSC_FREQ_HZ = 25'000'000.0;

// I2C write
bool write8(int fd, uint8_t reg, uint8_t val) {
    uint8_t buf[2] = {reg, val};
    return ::write(fd, buf, 2) == 2;
}

bool read8(int fd, uint8_t reg, uint8_t &val) {
    if (::write(fd, &reg, 1) != 1) return false;
    uint8_t b{};
    if (::read(fd, &b, 1) != 1) return false;
    val = b;
    return true;
}

bool write4(int fd, uint8_t reg, uint16_t on, uint16_t off) {
    // Little-endian for registers: ON_L, ON_H, OFF_L, OFF_H
    uint8_t buf[5] = {
        reg,
        static_cast<uint8_t>(on & 0xFF),
        static_cast<uint8_t>((on >> 8) & 0x0F),
        static_cast<uint8_t>(off & 0xFF),
        static_cast<uint8_t>((off >> 8) & 0x0F)
    };
    return ::write(fd, buf, 5) == 5;
}

bool setTick(int fd, uint8_t channel, uint16_t tick) {
    if (channel > 15) return false;
    uint8_t base = LED0_ON_L + 4 * channel;
    // Set ON=0 (cycle begin), OFF=tick
    return write4(fd, base, 0, tick);
}

bool allOff(int fd) {
    // All channels: ON=0, OFF=0, output in low state
    return write4(fd, ALL_LED_ON_L, 0, 0);
}

bool setPWMFreq(int fd, int hertz) {
    // PRESCALE = round(OSC/(4096*freq)) - 1
    double prescale_f = (OSC_FREQ_HZ / (4096.0 * static_cast<double>(hertz))) - 1.0;
    uint8_t prescale = static_cast<uint8_t>(std::round(prescale_f));
    uint8_t oldmode{};
    if (!read8(fd, MODE1, oldmode)) return false;

    uint8_t sleepmode = (oldmode & ~RESTART) | SLEEP; // sleep to save PRESCALE
    if (!write8(fd, MODE1, sleepmode)) return false;
    if (!write8(fd, PRESCALE, prescale)) return false;

    // Get back to work mode + Auto-Increment
    if (!write8(fd, MODE1, (oldmode & ~SLEEP) | AI)) return false;
    // MODE2: totem pole, update ACK 
    if (!write8(fd, MODE2, OUTDRV | OCH)) return false;

    // short pause
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    return true;
}
} // namespace