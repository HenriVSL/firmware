// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Henri Lavikainen

#pragma once

#include "dmm/services/rtty.h"

#include <cstddef>
#include <cstdint>
#include <span>

namespace dmm::services {

/// RTTY transmit audio: two-tone FSK without a phase jump at the changes,
/// as unsigned output levels (for example PWM duty) at a fixed sample rate.
/// Each character goes out as one start bit (space), five data bits (bit 0
/// first) and 1.5 stop bits (mark), after a lead of steady mark for the
/// transmitter and the far receiver to settle, and with a tail of mark
/// after the last one.
///
/// The level is worked out to 1/256 of an output step and the remainder
/// carried into the next sample, which moves the rounding noise of a low
/// output level above the audio band.
///
/// start() runs in a task while the modulator is idle; next() and
/// running() in the sample interrupt.
class RttyModulator
{
public:
    struct Config
    {
        std::uint16_t markHz = 2125;
        std::uint16_t shiftHz = rtty::ShiftHz;
        float baud = 45.45F;
        /// Mark is the higher tone.
        bool reverse = false;
    };

    /// Starts sending @p codes (5-bit ITA2, unchanged until sent) with
    /// @p leadMs of mark before and @p tailMs after, at @p peak output level.
    void start(std::span<const std::uint8_t> codes, const Config &config, std::uint16_t leadMs, std::uint16_t tailMs,
        std::uint32_t sampleRateHz, std::uint8_t peak);

    /// Ends at once: the next sample is 0.
    void stop() { m_running = false; }

    /// True until the tail's last sample.
    bool running() const { return m_running; }

    /// The next output level; 0 when not running.
    std::uint8_t next();

    /// How long sending @p characters takes with that lead and tail, ms.
    static std::uint32_t airMs(std::size_t characters, float baud, std::uint16_t leadMs, std::uint16_t tailMs);

private:
    void nextHalfBit();

    std::span<const std::uint8_t> m_codes;
    std::size_t m_index = 0;
    // Half bits of mark left in the lead, and in the tail once the codes
    // are out; the half bit within the character (0..14).
    std::uint32_t m_leadHalves = 0;
    std::uint32_t m_tailHalves = 0;
    std::uint8_t m_half = 0;
    std::uint32_t m_phase = 0;
    std::uint32_t m_markStep = 0;
    std::uint32_t m_spaceStep = 0;
    std::uint32_t m_halfPhase = 0;
    std::uint32_t m_halfStep = 0;
    std::uint16_t m_remainder = 0;
    std::uint8_t m_peak = 0;
    bool m_space = false;
    volatile bool m_running = false;
};

} // namespace dmm::services
