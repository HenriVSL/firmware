// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Henri Lavikainen

#pragma once

#include "dmm/services/rtty.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

/// Made-up received RTTY audio for the demodulator tests, at 9600 Hz:
/// phase-continuous two-tone FSK with a chosen level for each tone, the
/// sender's tones and speed off by a set amount, and noise, clicks and the
/// radio's DC level and clipping added after.
class RttySignal
{
public:
    static constexpr double RateHz = 9600.0;

    double markHz = 2125.0;
    double shiftHz = 170.0;
    double baud = 45.45;
    /// Added to both tones (the sender's tuning).
    double toneOffsetHz = 0.0;
    /// The sender's bit rate times this.
    double clockScale = 1.0;
    double stopBits = 1.5;
    double markAmplitude = 1.0;
    double spaceAmplitude = 1.0;
    /// The higher tone is mark.
    bool reverse = false;

    /// Sets the levels for a tilt of @p db (space louder than mark for a
    /// positive value) at the same total power.
    void setTilt(double db)
    {
        const double ratio = std::pow(10.0, db / 20.0);
        markAmplitude = std::sqrt(2.0 / (1.0 + ratio * ratio));
        spaceAmplitude = markAmplitude * ratio;
    }

    /// The mean power of the tones while sending.
    double signalPower() const { return (markAmplitude * markAmplitude + spaceAmplitude * spaceAmplitude) / 4.0; }

    /// No tone for @p seconds.
    void silence(double seconds) { m_samples.insert(m_samples.end(), static_cast<std::size_t>(seconds * RateHz), 0.0); }

    /// Steady mark for @p seconds (idle between characters).
    void idle(double seconds) { tone(true, seconds * baud * clockScale); }

    /// One character: start bit, five data bits (the first in bit 0), stop.
    void code(std::uint8_t c)
    {
        tone(false, 1.0);
        for (int b = 0; b < 5; ++b) {
            tone(((c >> b) & 1) != 0, 1.0);
        }
        tone(true, stopBits);
    }

    /// @p text as ITA2, after a letters shift, with idle mark around it.
    void text(std::string_view text, double leadS = 0.5, double tailS = 0.3)
    {
        idle(leadS);
        dmm::services::rtty::Ita2Encoder encoder;
        code(dmm::services::rtty::Letters);
        for (const char c : text) {
            std::uint8_t codes[2];
            const std::size_t n = encoder.encode(c, codes);
            for (std::size_t i = 0; i < n; ++i) {
                code(codes[i]);
            }
        }
        idle(tailS);
    }

    /// Adds noise at @p snrDb below signalPower() in a 3 kHz band: white
    /// (so its whole power over 4.8 kHz is 1.6 times that), or rising with
    /// frequency like an FM receiver's noise without de-emphasis (@p fm),
    /// scaled to the same power at the tones' frequency.
    void addNoise(double snrDb, std::uint32_t seed, bool fm = false)
    {
        m_seed = seed | 1u;
        const double inBand = signalPower() / std::pow(10.0, snrDb / 10.0);
        double sigma = std::sqrt(inBand * (RateHz / 2.0) / 3000.0);
        if (fm) {
            // The difference of white noise has a gain of 2 sin(pi f / fs).
            const double g = 2.0 * std::sin(Pi * (markHz + shiftHz / 2.0) / RateHz);
            sigma /= g;
        }
        double previous = 0.0;
        for (double &s : m_samples) {
            const double n = sigma * gaussian();
            s += fm ? n - previous : n;
            previous = n;
        }
    }

    /// Adds clicks, as an FM receiver near its threshold gives: about
    /// @p perSecond short pulses of @p amplitude, either sign.
    void addClicks(double perSecond, double amplitude, std::uint32_t seed)
    {
        m_seed = seed | 1u;
        const double p = perSecond / RateHz;
        for (std::size_t i = 0; i + 3 < m_samples.size(); ++i) {
            if (uniform() < p) {
                const double a = uniform() < 0.5 ? -amplitude : amplitude;
                m_samples[i] += 0.5 * a;
                m_samples[i + 1] += a;
                m_samples[i + 2] += 0.5 * a;
            }
        }
    }

    /// Scales the samples from @p from on by @p gain (a level step).
    void scaleFrom(std::size_t from, double gain)
    {
        for (std::size_t i = from; i < m_samples.size(); ++i) {
            m_samples[i] *= gain;
        }
    }

    /// The samples in mV around @p dcMv as the radio's ADC gives them,
    /// @p peakMv per unit of amplitude, cut at 0 and 3300 mV.
    std::vector<std::uint16_t> millivolts(double peakMv, double dcMv = 1110.0) const
    {
        std::vector<std::uint16_t> out;
        out.reserve(m_samples.size());
        for (double s : m_samples) {
            const double v = std::clamp(dcMv + s * peakMv + 0.5, 0.0, 3300.0);
            out.push_back(static_cast<std::uint16_t>(v));
        }
        return out;
    }

    std::vector<double> &samples() { return m_samples; }
    const std::vector<double> &samples() const { return m_samples; }

private:
    static constexpr double Pi = 3.14159265358979323846;

    // @p bits of one tone, keeping the phase.
    void tone(bool mark, double bits)
    {
        const bool high = mark == reverse;
        const double hz = (markHz + (high ? shiftHz : 0.0) + toneOffsetHz);
        const double amplitude = mark ? markAmplitude : spaceAmplitude;
        m_due += bits * RateHz / (baud * clockScale);
        while (m_due >= 1.0) {
            m_due -= 1.0;
            m_phase += 2.0 * Pi * hz / RateHz;
            if (m_phase > 2.0 * Pi) {
                m_phase -= 2.0 * Pi;
            }
            m_samples.push_back(amplitude * std::sin(m_phase));
        }
    }

    double uniform()
    {
        m_seed ^= m_seed << 13;
        m_seed ^= m_seed >> 17;
        m_seed ^= m_seed << 5;
        return (static_cast<double>(m_seed) + 1.0) / 4294967297.0;
    }

    double gaussian() { return std::sqrt(-2.0 * std::log(uniform())) * std::cos(2.0 * Pi * uniform()); }

    std::vector<double> m_samples;
    double m_phase = 0.0;
    double m_due = 0.0;
    std::uint32_t m_seed = 1;
};

/// Characters to turn @p a into @p b (insertions, deletions, changes).
inline std::size_t editDistance(std::string_view a, std::string_view b)
{
    std::vector<std::size_t> row(b.size() + 1);
    for (std::size_t j = 0; j <= b.size(); ++j) {
        row[j] = j;
    }
    for (std::size_t i = 1; i <= a.size(); ++i) {
        std::size_t diagonal = row[0];
        row[0] = i;
        for (std::size_t j = 1; j <= b.size(); ++j) {
            const std::size_t up = row[j];
            row[j] = std::min({row[j] + 1, row[j - 1] + 1, diagonal + (a[i - 1] == b[j - 1] ? 0 : 1)});
            diagonal = up;
        }
    }
    return row[b.size()];
}
