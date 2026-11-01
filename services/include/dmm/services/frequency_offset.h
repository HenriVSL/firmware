// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Henri Lavikainen

#pragma once

#include "dmm/lib/format.h"
#include "dmm/services/fm_transceiver.h"

#include <cstdint>

namespace dmm::services {

/// A shift added to every frequency the FM transceiver tunes, receive and
/// transmit, FM and DMR alike, for a radio whose frequency reference has
/// drifted: with it the radio is on the nominal frequencies again. The
/// shift is the same number of hertz on every band, while a reference's
/// error grows with the frequency.
///
/// The offset comes in whole steps: the smallest multiple of the
/// transceiver's tuning step that is at least MinStepMilliHz, so every step
/// moves the frequency and each value shown is the one applied.
class FrequencyOffset
{
public:
    /// The largest offset either way.
    static constexpr std::int32_t MaxHz = 5'000;
    static constexpr std::uint32_t MinStepMilliHz = 10'000;

    /// @p tuningStepMilliHz: the transceiver's tuning step.
    explicit constexpr FrequencyOffset(std::uint32_t tuningStepMilliHz)
        : m_stepMilliHz(stepFor(tuningStepMilliHz))
    {
    }

    constexpr std::uint32_t stepMilliHz() const { return m_stepMilliHz; }
    /// The most steps either way.
    constexpr int maxSteps() const { return static_cast<int>(static_cast<std::uint32_t>(MaxHz) * 1000 / m_stepMilliHz); }

    /// The offset in steps, -maxSteps()..maxSteps().
    constexpr int steps() const { return m_steps; }
    /// Clamped to the range.
    constexpr void setSteps(int steps) { m_steps = static_cast<int>(clampSteps(steps)); }
    /// The step nearest @p hz, clamped to the range.
    constexpr void setHz(std::int32_t hz) { setSteps(nearestSteps(static_cast<std::int64_t>(hz) * 1000)); }

    /// The offset applied, exactly.
    constexpr std::int32_t milliHz() const { return m_steps * static_cast<std::int32_t>(m_stepMilliHz); }
    /// The offset applied, to the nearest hertz: what is kept.
    constexpr std::int32_t hz() const
    {
        const std::int32_t m = milliHz();
        return m < 0 ? -((-m + 500) / 1000) : (m + 500) / 1000;
    }

    /// @p channel as the transceiver tunes it: the offset added to both its
    /// frequencies. A frequency of 0 (none) stays 0.
    FmChannel apply(FmChannel channel) const;

    /// The offset applied, to 0.1 Hz: "+1937.5 Hz", "0 Hz", "-2000 Hz".
    lib::StaticString<16> text() const;

private:
    static constexpr std::uint32_t stepFor(std::uint32_t tuningStepMilliHz)
    {
        const std::uint32_t tuning = tuningStepMilliHz == 0 ? 1 : tuningStepMilliHz;
        return (MinStepMilliHz + tuning - 1) / tuning * tuning;
    }
    constexpr int nearestSteps(std::int64_t milliHz) const
    {
        const std::int64_t step = m_stepMilliHz;
        return static_cast<int>(clampSteps(milliHz < 0 ? -((-milliHz + step / 2) / step) : (milliHz + step / 2) / step));
    }
    constexpr std::int64_t clampSteps(std::int64_t steps) const
    {
        if (steps < -maxSteps()) {
            return -maxSteps();
        }
        return steps > maxSteps() ? maxSteps() : steps;
    }

    std::uint32_t m_stepMilliHz;
    int m_steps = 0;
};

} // namespace dmm::services
