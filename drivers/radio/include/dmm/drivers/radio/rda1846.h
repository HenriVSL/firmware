// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tommi Tauriainen

#pragma once

#include "dmm/hal/register_bus.h"

#include <cstdint>

namespace dmm::drivers::radio {

/// The RDA1846 (AT1846S) FM transceiver with a 26 MHz reference. Register
/// meanings and start-up values from the chip maker's programming guide
/// and register table.
class Rda1846
{
public:
    static constexpr std::uint8_t ChipIdRegister = 0x00;
    static constexpr std::uint16_t ChipId = 0x1846;
    static constexpr std::uint8_t GpioRegister = 0x1F;
    static constexpr std::uint8_t FlagsRegister = 0x1C;
    static constexpr std::uint8_t QualityRegister = 0x21;

    // Bits of the flags register.
    static constexpr std::uint16_t SquelchOpen = 1u << 0;
    static constexpr std::uint16_t ToneMatch = 1u << 2;

    enum class Bandwidth : std::uint8_t
    {
        Narrow, // 12.5 kHz channels
        Wide,   // 25 kHz channels
    };

    enum class Mode : std::uint8_t
    {
        Idle,
        Receive,
        Transmit,
    };

    /// What a GPIO pin does: two bits per pin in the GPIO register.
    enum class GpioMode : std::uint8_t
    {
        HighImpedance = 0,
        Function = 1, // the pin's built-in signal
        Low = 2,
        High = 3,
    };

    explicit Rda1846(hal::RegisterBus &bus);

    /// True when the chip answers with its ID.
    bool present();

    /// Resets the chip and loads its start-up settings, narrow and idle.
    /// The reset clears the GPIO register; @p gpio is written back at once,
    /// before the slow calibration steps. @p sleepMs waits; about 170 ms in
    /// all. False when the chip does not answer.
    bool start(void (*sleepMs)(unsigned), std::uint16_t gpio);

    /// Tunes to @p hz. Change it only while idle.
    void setFrequency(std::uint32_t hz);
    /// Filters and gains for the channel width. Change it only while idle.
    void setBandwidth(Bandwidth bandwidth);
    /// Idle, receive or transmit, always passing through idle, with the
    /// drive and gain settings for that direction.
    void setMode(Mode mode);
    Mode mode() const { return m_mode; }

    /// Squelch opens above @p openDbm and closes below @p closeDbm.
    void setSquelch(int openDbm, int closeDbm);
    /// The squelch's noise condition: it opens when the received noise
    /// falls below @p openDb and closes when it rises above @p closeDb.
    void setNoiseSquelch(int openDb, int closeDb);
    /// The flags register: squelch, tone match.
    std::uint16_t flags() { return m_bus.read(FlagsRegister); }
    /// Received signal strength in dBm.
    int rssiDbm();
    /// Received noise in dB.
    int noiseDb();
    /// A receive quality figure, 0..15, lower for a strong clean signal
    /// (low four bits of register 0x21, not in the programming guide).
    int signalQuality() { return m_bus.read(QualityRegister) & 0xF; }

    /// CTCSS tone sent with transmit in 0.01 Hz units; 0 for none.
    void setTransmitTone(std::uint16_t centiHz);
    /// Transmit deviation: @p voice for voice and tone together (10 bits),
    /// @p tone for the tone alone (6 bits). Larger is wider.
    void setDeviation(std::uint16_t voice, std::uint8_t tone);

    std::uint16_t readRegister(std::uint8_t address) { return m_bus.read(address); }
    void writeRegister(std::uint8_t address, std::uint16_t value) { m_bus.write(address, value); }

    /// The frequency in the chip's 1/16 kHz units.
    static std::uint32_t frequencyWord(std::uint32_t hz);
    /// The squelch register for open and close levels in dBm.
    static std::uint16_t squelchWord(int openDbm, int closeDbm);

    /// Changes one GPIO pin (0..7), keeping the others as the chip has
    /// them. Returns the register value written.
    std::uint16_t setGpio(unsigned pin, GpioMode mode);

private:
    hal::RegisterBus &m_bus;
    Bandwidth m_bandwidth = Bandwidth::Narrow;
    Mode m_mode = Mode::Idle;
};

} // namespace dmm::drivers::radio
