// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Henri Lavikainen

#include "dmm/services/rtty_modulator.h"

#include "dmm/lib/tone_synth.h"

namespace dmm::services {

namespace {

std::uint32_t phaseStep(float hz, std::uint32_t sampleRateHz)
{
    const float rate = static_cast<float>(sampleRateHz == 0 ? 1 : sampleRateHz);
    const float step = hz / rate * 4294967296.0F;
    return step >= 4294967295.0F ? 0xFFFFFFFFu : step <= 0.0F ? 0u
                                                              : static_cast<std::uint32_t>(step);
}

// Half bits in a character: start 2, data 10, stop 3.
constexpr std::uint8_t CharacterHalves = 15;
constexpr std::uint8_t StopAt = 12;

std::uint32_t halvesIn(std::uint16_t ms, float baud)
{
    return static_cast<std::uint32_t>(static_cast<float>(ms) * 2.0F * baud / 1000.0F + 0.5F);
}

} // namespace

void RttyModulator::start(std::span<const std::uint8_t> codes, const Config &config, std::uint16_t leadMs,
    std::uint16_t tailMs, std::uint32_t sampleRateHz, std::uint8_t peak)
{
    m_running = false;
    const float baud = config.baud >= 10.0F && config.baud <= 300.0F ? config.baud : 45.45F;
    const float low = static_cast<float>(config.markHz);
    const float high = low + static_cast<float>(config.shiftHz);
    m_markStep = phaseStep(config.reverse ? high : low, sampleRateHz);
    m_spaceStep = phaseStep(config.reverse ? low : high, sampleRateHz);
    m_halfStep = phaseStep(2.0F * baud, sampleRateHz);
    m_codes = codes;
    m_index = 0;
    m_half = 0;
    // At least one half bit of mark before the first start bit.
    m_leadHalves = halvesIn(leadMs, baud);
    m_leadHalves = m_leadHalves == 0 ? 1 : m_leadHalves;
    m_tailHalves = halvesIn(tailMs, baud);
    m_halfPhase = 0;
    m_phase = 0;
    m_remainder = 0;
    m_peak = peak;
    m_running = true;
    nextHalfBit();
}

void RttyModulator::nextHalfBit()
{
    if (m_leadHalves > 0) {
        --m_leadHalves;
        m_space = false;
        return;
    }
    if (m_index < m_codes.size()) {
        const std::uint8_t h = m_half;
        if (h < 2) {
            m_space = true;
        } else if (h < StopAt) {
            m_space = ((m_codes[m_index] >> ((h - 2) / 2)) & 1) == 0;
        } else {
            m_space = false;
        }
        if (++m_half == CharacterHalves) {
            m_half = 0;
            ++m_index;
        }
        return;
    }
    if (m_tailHalves > 0) {
        --m_tailHalves;
        m_space = false;
        return;
    }
    m_running = false;
}

std::uint8_t RttyModulator::next()
{
    if (!m_running) {
        return 0;
    }
    m_phase += m_space ? m_spaceStep : m_markStep;
    // The wave between two table entries, in 1/256 steps.
    const std::uint32_t at = m_phase >> 26;
    const auto fraction = static_cast<std::int32_t>((m_phase >> 18) & 0xFF);
    const std::int32_t a = lib::ToneSynth::Wave[at];
    const std::int32_t b = lib::ToneSynth::Wave[(at + 1) % lib::ToneSynth::TableSize];
    const auto wave = static_cast<std::uint32_t>(a * 256 + (b - a) * fraction);
    const std::uint32_t level = ((wave * m_peak) >> 8) + m_remainder;
    m_remainder = static_cast<std::uint16_t>(level & 0xFF);
    const std::uint32_t previous = m_halfPhase;
    m_halfPhase = previous + m_halfStep;
    if (m_halfPhase < previous) {
        nextHalfBit();
    }
    return static_cast<std::uint8_t>(level >> 8);
}

std::uint32_t RttyModulator::airMs(std::size_t characters, float baud, std::uint16_t leadMs, std::uint16_t tailMs)
{
    return static_cast<std::uint32_t>(leadMs) + tailMs + rtty::charactersMs(characters, baud);
}

} // namespace dmm::services
