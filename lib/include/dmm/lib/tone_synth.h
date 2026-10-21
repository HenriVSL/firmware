// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tommi Tauriainen

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace dmm::lib {

namespace detail {

// cos(x) for 0 <= x <= 2 pi, from its Taylor series around pi.
constexpr double cosine(double x)
{
    constexpr double Pi = 3.14159265358979323846;
    const double y = x - Pi;
    double term = 1.0;
    double sum = 1.0;
    for (int n = 1; n < 14; ++n) {
        term *= -y * y / ((2 * n - 1) * (2 * n));
        sum += term;
    }
    return -sum;
}

// One cycle of (1 - cos) / 2, scaled to 0..255.
template <std::size_t N>
constexpr std::array<std::uint8_t, N> raisedCosine()
{
    constexpr double Pi = 3.14159265358979323846;
    std::array<std::uint8_t, N> wave{};
    for (std::size_t i = 0; i < N; ++i) {
        const double v = (1.0 - cosine(2.0 * Pi * static_cast<double>(i) / N)) / 2.0;
        wave[i] = static_cast<std::uint8_t>(v * 255.0 + 0.5);
    }
    return wave;
}

} // namespace detail

/// One tone by direct digital synthesis, as unsigned output levels (for
/// example PWM duty) from an interrupt at a fixed sample rate. The wave is a
/// raised cosine: every cycle starts and ends at 0, so a tone that starts
/// and stops on a cycle boundary does not click.
///
/// start() and stop() run in a task, next() in the sample interrupt.
class ToneSynth
{
public:
    static constexpr std::size_t TableSize = 64;
    static constexpr std::uint8_t MaxPeak = 255;

    /// Starts a tone, or changes the pitch and level of a running one
    /// without a phase jump. @p peak is the highest output, 0..255.
    void start(std::uint32_t hz, std::uint32_t sampleRateHz, std::uint8_t peak)
    {
        const std::uint64_t step = (static_cast<std::uint64_t>(hz) << 32) / (sampleRateHz == 0 ? 1 : sampleRateHz);
        m_step = static_cast<std::uint32_t>(step > 0x7FFFFFFFu ? 0x7FFFFFFFu : step);
        m_peak = peak;
        m_stopping = false;
        m_running = m_step != 0;
    }

    /// Ends the tone when the current cycle is over.
    void stop() { m_stopping = true; }

    /// True until a stopped tone has finished its last cycle.
    bool running() const { return m_running; }

    /// The next output level; 0 when not running.
    std::uint8_t next()
    {
        if (!m_running) {
            return 0;
        }
        const std::uint32_t previous = m_phase;
        m_phase = previous + m_step;
        if (m_stopping && m_phase < previous) {
            m_running = false;
            m_phase = 0;
            return 0;
        }
        return static_cast<std::uint8_t>((Wave[m_phase >> 26] * m_peak) >> 8);
    }

public:
    static constexpr std::array<std::uint8_t, TableSize> Wave = detail::raisedCosine<TableSize>();

private:
    volatile std::uint32_t m_phase = 0;
    volatile std::uint32_t m_step = 0;
    volatile std::uint8_t m_peak = 0;
    volatile bool m_stopping = false;
    volatile bool m_running = false;
};

static_assert(ToneSynth::Wave[0] == 0 && ToneSynth::Wave[ToneSynth::TableSize / 2] == 255);

} // namespace dmm::lib
