// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Henri Lavikainen

#include "dmm/services/audio_meter.h"

#include <cmath>

namespace dmm::services {

AudioMeter::AudioMeter(std::uint32_t sampleRateHz, std::span<const std::uint32_t> tonesHz, std::uint32_t windowSamples)
    : m_windowSamples(windowSamples == 0 ? 1 : windowSamples)
{
    constexpr double Pi = 3.14159265358979323846;
    for (std::uint32_t hz : tonesHz) {
        if (m_toneCount == MaxTones || sampleRateHz == 0) {
            break;
        }
        m_tones[m_toneCount++].coefficient =
            static_cast<float>(2.0 * std::cos(2.0 * Pi * static_cast<double>(hz) / static_cast<double>(sampleRateHz)));
    }
}

void AudioMeter::add(std::uint16_t mv)
{
    if (m_count == 0) {
        m_offset = static_cast<float>(mv);
    }
    ++m_count;
    m_sum += mv;
    m_sumSquares += static_cast<std::uint64_t>(mv) * mv;
    m_min = mv < m_min ? mv : m_min;
    m_max = mv > m_max ? mv : m_max;

    // Goertzel: one resonator per tone, read out at the window's end.
    const float x = static_cast<float>(mv) - m_offset;
    for (std::size_t i = 0; i < m_toneCount; ++i) {
        Tone &t = m_tones[i];
        const float s = x + t.coefficient * t.s1 - t.s2;
        t.s2 = t.s1;
        t.s1 = s;
    }
    if (++m_inWindow == m_windowSamples) {
        endWindow();
    }
}

void AudioMeter::endWindow()
{
    for (std::size_t i = 0; i < m_toneCount; ++i) {
        Tone &t = m_tones[i];
        const float power = t.s1 * t.s1 + t.s2 * t.s2 - t.coefficient * t.s1 * t.s2;
        t.powerSum += power;
        t.s1 = 0.0F;
        t.s2 = 0.0F;
    }
    m_inWindow = 0;
    ++m_windows;
}

AudioMeter::Result AudioMeter::result() const
{
    Result r;
    r.samples = m_count;
    r.windows = m_windows;
    if (m_count == 0) {
        return r;
    }
    const double n = m_count;
    const double mean = static_cast<double>(m_sum) / n;
    const double variance = static_cast<double>(m_sumSquares) / n - mean * mean;
    r.meanMv = static_cast<int>(mean + 0.5);
    r.minMv = m_min;
    r.maxMv = m_max;
    r.acRmsMv = static_cast<float>(std::sqrt(variance > 0.0 ? variance : 0.0));
    if (m_windows > 0) {
        // A sine of amplitude A on a window of N samples gives a power of
        // (A N / 2)^2, so its RMS level is sqrt(2 power) / N.
        const double windowLength = m_windowSamples;
        for (std::size_t i = 0; i < m_toneCount; ++i) {
            const double power = m_tones[i].powerSum / m_windows;
            r.toneRmsMv[i] = static_cast<float>(std::sqrt(2.0 * (power > 0.0 ? power : 0.0)) / windowLength);
        }
    }
    return r;
}

float AudioMeter::ratioDb(float a, float b)
{
    constexpr float Limit = 99.0F;
    if (a <= 0.0F && b <= 0.0F) {
        return 0.0F;
    }
    if (b <= 0.0F) {
        return Limit;
    }
    if (a <= 0.0F) {
        return -Limit;
    }
    const float db = 20.0F * std::log10(a / b);
    return db > Limit ? Limit : db < -Limit ? -Limit
                                            : db;
}

} // namespace dmm::services
