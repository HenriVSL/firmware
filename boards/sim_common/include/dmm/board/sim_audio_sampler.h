// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Henri Lavikainen

#pragma once

#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <span>

/// Received audio made up as it is read: 1200 baud AFSK (1200 and 2200 Hz)
/// with random bits, 300 mV peak on 1100 mV, plus a little noise, at the
/// rate asked for and in real time.
class SimAudioSampler
{
public:
    bool start(std::uint32_t rateHz)
    {
        if (rateHz < 4800 || rateHz > 96'000) {
            return false;
        }
        m_rateHz = rateHz;
        m_start = Clock::now();
        m_made = 0;
        m_lost = 0;
        m_phase = 0.0;
        m_bitPhase = 0.0;
        m_space = false;
        m_running = true;
        return true;
    }

    std::size_t read(std::span<std::uint16_t> out)
    {
        if (!m_running) {
            return 0;
        }
        const auto elapsedUs = std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - m_start).count();
        const auto due = static_cast<std::uint64_t>(elapsedUs) * m_rateHz / 1'000'000;
        // A radio keeps this many; older ones are gone.
        constexpr std::uint64_t Kept = 2048;
        if (due - m_made > Kept) {
            const std::uint64_t skipped = due - m_made - Kept;
            for (std::uint64_t i = 0; i < skipped; ++i) {
                next();
            }
            m_lost += static_cast<std::uint32_t>(skipped);
        }
        std::size_t n = 0;
        while (n < out.size() && m_made < due) {
            out[n++] = next();
        }
        return n;
    }

    std::uint32_t lostSamples() const { return m_lost; }
    void stop() { m_running = false; }

private:
    using Clock = std::chrono::steady_clock;

    std::uint16_t next()
    {
        constexpr double Pi = 3.14159265358979323846;
        ++m_made;
        m_bitPhase += 1200.0 / m_rateHz;
        if (m_bitPhase >= 1.0) {
            m_bitPhase -= 1.0;
            // NRZI: a 0 bit changes the tone.
            if ((random() & 1u) == 0) {
                m_space = !m_space;
            }
        }
        m_phase += 2.0 * Pi * (m_space ? 2200.0 : 1200.0) / m_rateHz;
        if (m_phase >= 2.0 * Pi) {
            m_phase -= 2.0 * Pi;
        }
        const double noise = static_cast<double>(random() % 41) - 20.0;
        return static_cast<std::uint16_t>(1100.0 + 300.0 * std::sin(m_phase) + noise + 0.5);
    }

    std::uint32_t random()
    {
        m_seed = m_seed * 1664525u + 1013904223u;
        return m_seed >> 16;
    }

    Clock::time_point m_start{};
    std::uint64_t m_made = 0;
    std::uint32_t m_rateHz = 0;
    std::uint32_t m_lost = 0;
    std::uint32_t m_seed = 1;
    double m_phase = 0.0;
    double m_bitPhase = 0.0;
    bool m_space = false;
    bool m_running = false;
};
