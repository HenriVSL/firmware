// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tommi Tauriainen

#include "dmm/drivers/radio/rda1846.h"

#include <span>

namespace dmm::drivers::radio {

namespace {

struct Setting
{
    std::uint8_t reg;
    std::uint16_t value;
};

// The chip maker's start-up list for a 26 MHz reference (soft reset and
// the calibration run are done around it).
constexpr Setting StartUp[] = {
    {0x04, 0x0FD0}, // 26 MHz reference
    {0x31, 0x0031},
    {0x33, 0x44A5}, // AGC
    {0x34, 0x2B87}, // receive digital gain
    {0x41, 0x470F}, // transmit voice digital gain
    {0x44, 0x0DFF}, // transmit gain, receive DAC gain and volume
    {0x47, 0x7FFF}, // soft mute
    {0x4F, 0x2C62},
    {0x53, 0x0094},
    {0x55, 0x0081},
    {0x56, 0x0B22}, // squelch detect time
    {0x57, 0x1C00}, // RSSI filter bypass
    {0x5A, 0x0EDB}, // squelch detect time
    {0x60, 0x101E}, // squelch noise threshold
    {0x63, 0x16AD}, // pre-emphasis bypass threshold
};

// Per channel width, from the same list: {register, narrow, wide}. Page 1
// (selected through 0x7F) holds the AGC table.
struct WidthSetting
{
    std::uint8_t reg;
    std::uint16_t narrow;
    std::uint16_t wide;
};

constexpr WidthSetting WidthPage0[] = {
    {0x15, 0x1100, 0x1F00},
    {0x32, 0x4495, 0x7564}, // AGC
    {0x3A, 0x40C3, 0x44C3}, // transmit from the microphone; squelch detectors
    {0x3C, 0x0F1E, 0x172C}, // squelch peak detect
    {0x3F, 0x28D0, 0x29D2}, // squelch RSSI threshold
    {0x48, 0x20BE, 0x2141}, // squelch noise threshold
    {0x62, 0x0A10, 0x2346},
    {0x65, 0x2494, 0x248A},
    {0x66, 0xEB2E, 0xFF2E}, // RSSI compensation
};

constexpr WidthSetting WidthPage1[] = {
    {0x06, 0x0014, 0x0024},
    {0x07, 0x020C, 0x0214},
    {0x08, 0x0214, 0x0224},
    {0x09, 0x030C, 0x0314},
    {0x0A, 0x0314, 0x0324},
    {0x0B, 0x0324, 0x0344},
    {0x0C, 0x0344, 0x0384},
    {0x0D, 0x1344, 0x1384},
    {0x0E, 0x1B44, 0x1B84},
    {0x0F, 0x3F44, 0x3F84},
    {0x12, 0xE0EB, 0xE0EB},
};

constexpr std::uint8_t ControlRegister = 0x30;
constexpr std::uint8_t PageRegister = 0x7F;
constexpr std::uint8_t RssiRegister = 0x1B;
constexpr std::uint8_t BandRegister = 0x05;
constexpr std::uint8_t FrequencyHigh = 0x29;
constexpr std::uint8_t FrequencyLow = 0x2A;
constexpr std::uint8_t SquelchRegister = 0x49;
constexpr std::uint8_t NoiseSquelchRegister = 0x48;
constexpr std::uint8_t FiltersRegister = 0x58;
constexpr std::uint8_t DeviationRegister = 0x59;
constexpr std::uint8_t Ctcss1Register = 0x4A;
constexpr std::uint8_t ToneModeRegister = 0x4E;

// Control register: reference 26 MHz, chip powered, and for wide channels
// both the analog and the DSP filters at 25 kHz.
constexpr std::uint16_t ControlNarrow = 0x4006;
constexpr std::uint16_t ControlWide = 0x7006;
constexpr std::uint16_t ReceiveOn = 1u << 5;
constexpr std::uint16_t TransmitOn = 1u << 6;
// The chip maker's calibration run after reset, in two steps.
constexpr std::uint16_t CalibrateA = 0x40A4;
constexpr std::uint16_t CalibrateB = 0x40A6;

// Receive levels. 0x0A: the chip maker's narrow-channel value. 0x40: FM
// paths. 0x44: receive volume step 9 of 15, full DAC gain, transmit gain 9.
constexpr Setting Board[] = {
    {0x0A, 0x4380},
    {0x40, 0x0030},
    {0x44, 0x09F9},
};
constexpr Setting ReceiveLevels[] = {
    {0x0A, 0x4380},
    {0x44, 0x09F9},
};

// Transmit levels. 0x0A: driver step 7 (bits 14:11), the drive the power
// calibration is made at, and microphone amplifier gain 14. 0x41: voice
// digital gain. 0x44: transmit gain 9, full DAC gain and volume.
constexpr Setting TransmitLevels[] = {
    {0x0A, 0x3BA0},
    {0x41, 0x070F},
    {0x44, 0x09FF},
};

// Filters: in receive the baseband chip does the de-emphasis, so the
// transceiver's own is bypassed; transmit keeps its pre-emphasis. Both use
// the 250 Hz tone filter.
constexpr std::uint16_t ReceiveFilters = 0x8085;
constexpr std::uint16_t TransmitFilters = 0x8405;

// Band register: normal, or the setting for the few frequencies where the
// 26 MHz reference's harmonics fall (136.5, 409.5 and 455 MHz).
constexpr std::uint16_t BandNormal = 0x8763;
constexpr std::uint16_t BandSpecial = 0x8543;
constexpr std::uint32_t SpecialHz[] = {136'500'000, 409'500'000, 455'000'000};
constexpr std::uint32_t SpecialWindowHz = 1'000'000;

// Tone mode: no tone, or CTCSS from the chip (bits 10:9 = 11), no phase
// shift at the end.
constexpr std::uint16_t ToneModeOff = 0x2002;
constexpr std::uint16_t ToneModeCtcss = 0x2602;

constexpr int RssiOffsetDb = 137;

} // namespace

Rda1846::Rda1846(hal::RegisterBus &bus)
    : m_bus(bus)
{
}

bool Rda1846::present()
{
    return m_bus.read(ChipIdRegister) == ChipId;
}

std::uint16_t Rda1846::setGpio(unsigned pin, GpioMode mode)
{
    if (pin > 7) {
        return m_bus.read(GpioRegister);
    }
    const unsigned shift = 2 * pin;
    std::uint16_t value = m_bus.read(GpioRegister);
    value = static_cast<std::uint16_t>((value & ~(3u << shift)) | (static_cast<unsigned>(mode) << shift));
    m_bus.write(GpioRegister, value);
    return value;
}

bool Rda1846::start(void (*sleepMs)(unsigned), std::uint16_t gpio)
{
    if (!present()) {
        return false;
    }
    m_bus.write(ControlRegister, 0x0001); // soft reset
    sleepMs(50);
    m_bus.write(ControlRegister, 0x0004); // power on
    m_bus.write(GpioRegister, gpio);
    for (const Setting &s : StartUp) {
        m_bus.write(s.reg, s.value);
    }
    for (const Setting &s : Board) {
        m_bus.write(s.reg, s.value);
    }
    m_bus.write(ControlRegister, CalibrateA);
    sleepMs(50);
    m_bus.write(ControlRegister, CalibrateB);
    sleepMs(100);
    m_bus.write(ControlRegister, ControlNarrow);
    sleepMs(10);
    m_mode = Mode::Idle;
    setBandwidth(Bandwidth::Narrow);
    return true;
}

std::uint32_t Rda1846::frequencyWord(std::uint32_t hz)
{
    return static_cast<std::uint32_t>(static_cast<std::uint64_t>(hz) * 16 / 1000);
}

void Rda1846::setFrequency(std::uint32_t hz)
{
    bool special = false;
    for (std::uint32_t s : SpecialHz) {
        const std::uint32_t d = hz > s ? hz - s : s - hz;
        special = special || d <= SpecialWindowHz;
    }
    m_bus.write(BandRegister, special ? BandSpecial : BandNormal);
    const std::uint32_t word = frequencyWord(hz);
    m_bus.write(FrequencyHigh, static_cast<std::uint16_t>(word >> 16));
    m_bus.write(FrequencyLow, static_cast<std::uint16_t>(word));
}

void Rda1846::setBandwidth(Bandwidth bandwidth)
{
    m_bandwidth = bandwidth;
    const bool wide = bandwidth == Bandwidth::Wide;
    for (const WidthSetting &s : WidthPage0) {
        m_bus.write(s.reg, wide ? s.wide : s.narrow);
    }
    m_bus.write(PageRegister, 1);
    for (const WidthSetting &s : WidthPage1) {
        m_bus.write(s.reg, wide ? s.wide : s.narrow);
    }
    m_bus.write(PageRegister, 0);
}

void Rda1846::setMode(Mode mode)
{
    const std::uint16_t idle = m_bandwidth == Bandwidth::Wide ? ControlWide : ControlNarrow;
    m_bus.write(ControlRegister, idle);
    m_mode = mode;
    if (mode == Mode::Idle) {
        return;
    }
    const bool transmit = mode == Mode::Transmit;
    for (const Setting &s : transmit ? std::span<const Setting>(TransmitLevels) : std::span<const Setting>(ReceiveLevels)) {
        m_bus.write(s.reg, s.value);
    }
    m_bus.write(FiltersRegister, transmit ? TransmitFilters : ReceiveFilters);
    m_bus.write(ControlRegister, static_cast<std::uint16_t>(idle | (transmit ? TransmitOn : ReceiveOn)));
}

std::uint16_t Rda1846::squelchWord(int openDbm, int closeDbm)
{
    auto field = [](int dbm) {
        const int v = dbm + RssiOffsetDb;
        return static_cast<std::uint16_t>(v < 0 ? 0 : v > 0x7F ? 0x7F
                                                               : v);
    };
    return static_cast<std::uint16_t>(field(openDbm) << 7 | field(closeDbm));
}

void Rda1846::setSquelch(int openDbm, int closeDbm)
{
    m_bus.write(SquelchRegister, squelchWord(openDbm, closeDbm));
}

void Rda1846::setNoiseSquelch(int openDb, int closeDb)
{
    auto field = [](int db) { return static_cast<std::uint16_t>(db < 0 ? 0 : db > 0x7F ? 0x7F
                                                                                       : db); };
    m_bus.write(NoiseSquelchRegister, static_cast<std::uint16_t>(field(openDb) << 7 | field(closeDb)));
}

int Rda1846::noiseDb()
{
    return static_cast<int>(m_bus.read(RssiRegister) & 0xFF);
}

int Rda1846::rssiDbm()
{
    return static_cast<int>(m_bus.read(RssiRegister) >> 8) - RssiOffsetDb;
}

void Rda1846::setTransmitTone(std::uint16_t centiHz)
{
    if (centiHz == 0) {
        m_bus.write(ToneModeRegister, ToneModeOff);
        return;
    }
    m_bus.write(Ctcss1Register, centiHz);
    m_bus.write(ToneModeRegister, ToneModeCtcss);
}

void Rda1846::setDeviation(std::uint16_t voice, std::uint8_t tone)
{
    m_bus.write(DeviationRegister, static_cast<std::uint16_t>((voice & 0x3FF) << 6 | (tone & 0x3F)));
}

} // namespace dmm::drivers::radio
