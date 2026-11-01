// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Henri Lavikainen

#pragma once

#include "sim_aprs_stations.h"

#include "dmm/services/afsk.h"
#include "dmm/services/ax25.h"
#include "dmm/services/rtty.h"

#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

/// Received audio made up as it is read, in real time at the rate asked
/// for: at the start of each slot of sim_aprs a made-up station's packet
/// as 1200 baud AFSK (1200 and 2200 Hz, the higher tone 4 dB louder, as
/// from a sender with pre-emphasis), otherwise noise, on 1100 mV. With
/// setRtty() the audio is that text as RTTY instead (2125/2295 Hz,
/// 45.45 Bd), sent again and again with a pause between.
class SimAudioSampler
{
public:
    /// Sends @p text as RTTY rather than APRS packets.
    void setRtty(std::string_view text)
    {
        constexpr int Second = 91; // half bits at 45.45 Bd
        m_rtty.clear();
        auto add = [this](std::uint8_t level, int halves) { m_rtty.insert(m_rtty.end(), static_cast<std::size_t>(halves), level); };
        auto code = [&add](std::uint8_t c) {
            add(0, 2);
            for (int b = 0; b < 5; ++b) {
                add((c >> b) & 1, 2);
            }
            add(1, 3);
        };
        add(1, Second);
        code(dmm::services::rtty::Letters);
        dmm::services::rtty::Ita2Encoder encoder;
        for (const char c : text) {
            std::uint8_t codes[2];
            const std::size_t n = encoder.encode(c, codes);
            for (std::size_t i = 0; i < n; ++i) {
                code(codes[i]);
            }
        }
        add(1, Second / 2);
        add(Pause, 3 * Second);
    }

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
        m_sending = false;
        m_slot = sim_aprs::slotAt(m_start);
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
        constexpr std::uint64_t Kept = 4096;
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

    // A packet starts at each new slot.
    void startPacket()
    {
        const auto at = m_start + std::chrono::microseconds(m_made * 1'000'000 / m_rateHz);
        const std::int64_t slot = sim_aprs::slotAt(at);
        if (slot == m_slot) {
            return;
        }
        m_slot = slot;
        const std::size_t n = sim_aprs::frame(slot, m_frame);
        m_bits.start(std::span<const std::uint8_t>(m_frame.data(), n), 30, 2);
        m_bitPhase = 1.0;
        m_sending = true;
    }

    std::uint16_t next()
    {
        constexpr double Pi = 3.14159265358979323846;
        ++m_made;
        if (!m_sending && m_rtty.empty()) {
            startPacket();
        }
        double level = 0.0;
        if (!m_rtty.empty()) {
            m_halfPhase += 2.0 * 45.45 / m_rateHz;
            if (m_halfPhase >= 1.0) {
                m_halfPhase -= 1.0;
                m_rttyAt = (m_rttyAt + 1) % m_rtty.size();
            }
            const std::uint8_t now = m_rtty[m_rttyAt];
            if (now != Pause) {
                m_phase += 2.0 * Pi * (now == 1 ? 2125.0 : 2295.0) / m_rateHz;
                if (m_phase >= 2.0 * Pi) {
                    m_phase -= 2.0 * Pi;
                }
                level = 300.0 * std::sin(m_phase);
            }
        } else if (m_sending) {
            m_bitPhase += 1200.0 / m_rateHz;
            if (m_bitPhase >= 1.0) {
                m_bitPhase -= 1.0;
                const int bit = m_bits.next();
                if (bit < 0) {
                    m_sending = false;
                } else if (bit == 0) {
                    // NRZI: a 0 bit changes the tone.
                    m_space = !m_space;
                }
            }
            m_phase += 2.0 * Pi * (m_space ? 2200.0 : 1200.0) / m_rateHz;
            if (m_phase >= 2.0 * Pi) {
                m_phase -= 2.0 * Pi;
            }
            level = (m_space ? 380.0 : 240.0) * std::sin(m_phase);
        }
        const double noise = static_cast<double>(random() % 81) - 40.0;
        return static_cast<std::uint16_t>(1100.0 + level + noise + 0.5);
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
    bool m_sending = false;
    bool m_running = false;
    std::int64_t m_slot = 0;
    dmm::services::HdlcBits m_bits;
    std::array<std::uint8_t, dmm::services::ax25::MaxFrameBytes> m_frame{};
    // RTTY in half bits: 1 mark, 0 space, Pause no tone.
    static constexpr std::uint8_t Pause = 2;
    std::vector<std::uint8_t> m_rtty;
    std::size_t m_rttyAt = 0;
    double m_halfPhase = 0.0;
};
