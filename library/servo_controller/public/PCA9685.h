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
constexpr uint8_t PCA9685_ADDR   = 0x42;
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
bool write8(int fd, uint8_t reg, uint8_t val)
{
    uint8_t buf[2] = {reg, val};
    return ::write(fd, buf, 2) == 2;
}

// I2C read
bool read8(int fd, uint8_t reg, uint8_t &val)
{
    if (::write(fd, &reg, 1) != 1) return false;
    uint8_t b{};
    if (::read(fd, &b, 1) != 1) return false;
    val = b;
    return true;
}

bool write4(int fd, uint8_t reg, uint16_t on, uint16_t off)
{
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

bool setTick(int fd, uint8_t channel, uint16_t tick)
{
    tick = std::min<uint16_t>(tick, MAX_PWM);
    if (channel > 15) return false;
    uint8_t base = LED0_ON_L + 4 * channel;
    // Set ON=0 (cycle begin), OFF=tick
    return write4(fd, base, 0, tick);
}

bool allOff(int fd)
{
    // All channels: ON=0, OFF=0, output in low state
    return write4(fd, ALL_LED_ON_L, 0, 0);
}

double calc_real_freq_from_prescale(uint8_t prescale)
{
    // f = OSC / (4096 * (prescale + 1))
    return OSC_FREQ_HZ / (4096.0 * (static_cast<double>(prescale) + 1.0));
}

bool read_mode_regs(int fd, uint8_t& mode1, uint8_t& mode2, uint8_t& prescale)
{
    if (!read8(fd, MODE1, mode1)) return false;
    if (!read8(fd, MODE2, mode2)) return false;
    if (!read8(fd, PRESCALE, prescale)) return false;
    return true;
}

void print_pca_report(int fd, int target_hz, double tol_pct = 10.0)
{
    uint8_t m1{}, m2{}, ps{};
    if (!read_mode_regs(fd, m1, m2, ps)) {
        fprintf(stderr, "[PCA9685] read regs failed\n");
        return;
    }
    const double f_real = calc_real_freq_from_prescale(ps);
    const double err_pct = 100.0 * (f_real - target_hz) / target_hz;

    printf("[PCA9685] MODE1=0x%02X  MODE2=0x%02X  PRESCALE=%u\n", m1, m2, ps);
    printf("[PCA9685] freq(target=%d Hz) ≈ %.2f Hz  (err=%.2f%%)\n",
           target_hz, f_real, err_pct);

    const bool ai      = (m1 & AI)      != 0;
    const bool sleep   = (m1 & SLEEP)   != 0;
    const bool restart = (m1 & RESTART) != 0;
    const bool outdrv  = (m2 & OUTDRV)  != 0;
    const bool och     = (m2 & OCH)     != 0;

    printf("[PCA9685] AI=%d  SLEEP=%d  RESTART=%d  OUTDRV=%d  OCH=%d\n",
           ai, sleep, restart, outdrv, och);

    if (sleep)
    {
        printf("[PCA9685][WARN] SLEEP=1\n");
    }
    if (!ai)
    {
        printf("[PCA9685][WARN] AI=0\n");
    }
    if (!outdrv)
    {
        printf("[PCA9685][WARN] MODE2.OUTDRV=0\n");
    }

    const double tol = std::max(0.5, tol_pct);
    if (std::fabs(err_pct) > tol)
    {
        printf("[PCA9685][WARN] Frequency deviation > %.1f%% — check PRESCALE/oscillator.\n", tol);
    }
}

bool setPWMFreq(int fd, int hertz)
{
    double prescale_f = (OSC_FREQ_HZ / (4096.0 * static_cast<double>(hertz))) - 1.0;
    uint8_t prescale = static_cast<uint8_t>(std::round(prescale_f));

    uint8_t oldmode{};
    if (!read8(fd, MODE1, oldmode)) return false;

    uint8_t sleepmode = (oldmode & ~RESTART) | SLEEP;
    if (!write8(fd, MODE1, sleepmode)) return false;
    if (!write8(fd, PRESCALE, prescale)) return false;

    if (!write8(fd, MODE1, (oldmode & ~SLEEP) | AI)) return false;
    // MODE2
    if (!write8(fd, MODE2, OUTDRV)) return false;

    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    uint8_t mode1_after{};
    if (!read8(fd, MODE1, mode1_after)) return false;
    if (!write8(fd, MODE1, mode1_after | RESTART)) return false;

    return true;
}
} // namespace