// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tommi Tauriainen

#pragma once

#include "dmm/services/storage.h"

#include <array>
#include <cstdint>
#include <optional>
#include <span>

namespace dmm::services {

/// Transmit power levels, numbered as in the codeplug.
enum class PowerLevel : std::uint8_t
{
    Low = 0,
    Mid = 1,
    High = 2,
    Turbo = 3,
};

/// The radio's calibration: two copies of one 128 KB block, the first
/// 0x620 bytes used. A copy is valid when the last four bytes of its block
/// read 55 55 AA AA and the four before them do not read AA AA 55 55; when
/// the first copy is not valid, the second is used. Pages the flash flags
/// as uncorrectable are used as they are: these blocks are written in
/// pieces, which always fails the flash's own check. Read only.
class StockCalibration
{
public:
    static constexpr std::uint32_t CopyA = 0x02F60000;
    static constexpr std::uint32_t CopyB = 0x02F80000;
    static constexpr std::uint32_t BlockBytes = 0x20000;
    static constexpr std::uint32_t Bytes = 0x620;

    /// Reads the valid copy. False when storage could not be read.
    bool load(Storage &storage);

    bool loaded() const { return m_loaded; }
    /// Which copy was read: 0 = first, 1 = second.
    int copy() const { return m_copy; }
    /// The flash flagged the copy as uncorrectable.
    bool flagged() const { return m_flagged; }
    /// The radio was calibrated at the factory (its first-run marker is set).
    bool present() const { return m_loaded && m_data[0x14] == 'w' && m_data[0x15] == 'w'; }

    /// Uses @p bytes as the loaded copy, for tests and the simulator.
    void use(std::span<const std::uint8_t, Bytes> bytes);

    /// The battery reading's offset in millivolts, if calibrated.
    std::optional<int> batteryOffsetMv() const;

    // RF calibration. Calibrated bands: 136-174 MHz and 400-520 MHz; other
    // frequencies give no value. Every value needs present().

    /// Reference oscillator trim, a 12-bit DAC value.
    std::optional<std::uint16_t> referenceTrim() const;
    /// Power amplifier control at @p hz and @p level, a 12-bit DAC value:
    /// the band's base value (calibrated at 155 and 460 MHz) corrected by
    /// the level's per-MHz flatness table, an 8-bit result that sets the
    /// DAC's top eight bits. No value when the result leaves 0..255.
    std::optional<std::uint16_t> txPower(std::uint32_t hz, PowerLevel level) const;
    /// Receive front-end tuning at @p hz, a 12-bit PWM value interpolated
    /// between the band's three calibration points.
    std::optional<std::uint16_t> rxTune(std::uint32_t hz) const;
    /// Squelch threshold at @p hz for squelch level @p level (1..5; higher
    /// is tighter): the band's base corrected by its flatness table.
    std::optional<int> squelchThreshold(std::uint32_t hz, int level) const;

    /// FM transmit deviation settings, one set for both bands: voice (with
    /// tones), CTCSS and DCS.
    struct Deviation
    {
        std::uint8_t voice;
        std::uint8_t ctcss;
        std::uint8_t dcs;
    };
    std::optional<Deviation> deviation() const;

    /// Audio codec trims: left and right line-out gain, right input
    /// amplifier volume.
    struct CodecTrims
    {
        std::uint8_t leftOut;
        std::uint8_t rightOut;
        std::uint8_t rightInput;
    };
    std::optional<CodecTrims> codecTrims() const;

    /// The baseband chip's analog-mode gains, big-endian words: transmit
    /// before and after the limiter, CTCSS, DCS and receive volume.
    std::optional<std::array<std::uint8_t, 10>> basebandAnalogGains() const;

    const std::uint8_t *data() const { return m_data; }

private:
    bool m_loaded = false;
    int m_copy = 0;
    bool m_flagged = false;
    std::uint8_t m_data[Bytes]{};
};

} // namespace dmm::services
