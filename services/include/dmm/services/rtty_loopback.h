// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Henri Lavikainen

#pragma once

#include "dmm/services/rtty_modulator.h"

#include <cstddef>
#include <cstdint>
#include <span>

namespace dmm::services {

/// The transmit audio as the receiver's audio input would give it, for
/// testing the demodulator without a radio link: the modulator's output
/// levels at its own sample rate, each output sample the mean of the input
/// over its time (9600 Hz by default), scaled to ToneMv peak around DcMv,
/// with white noise added at a chosen signal-to-noise ratio in 3 kHz.
/// Heap-free; any task.
class RttyLoopback
{
public:
    static constexpr std::uint16_t DcMv = 1110;
    static constexpr std::uint16_t ToneMv = 300;

    /// Starts on @p codes (unchanged until done) as RttyModulator::start()
    /// takes them, at @p modulatorRateHz and @p peak. @p snrDb of 99 or
    /// more adds no noise; @p noiseFirstMs of noise alone comes first.
    void start(std::span<const std::uint8_t> codes, const RttyModulator::Config &config, std::uint16_t leadMs,
        std::uint16_t tailMs, std::uint32_t modulatorRateHz, std::uint8_t peak, float snrDb, std::uint16_t noiseFirstMs,
        std::uint32_t seed, std::uint32_t outputRateHz = 9600);

    /// Fills @p out with samples in mV; fewer than asked once the tail is
    /// out, then none.
    std::size_t read(std::span<std::uint16_t> out);

    bool running() const { return m_running; }

private:
    float noise();

    RttyModulator m_modulator;
    // Time is counted in units that both rates divide: an input sample
    // spans m_inUnits, an output sample m_outUnits.
    std::uint32_t m_inUnits = 1;
    std::uint32_t m_outUnits = 1;
    float m_sum = 0.0F;
    float m_carry = 0.0F;
    std::uint32_t m_carryUnits = 0;
    float m_scale = 0.0F;
    float m_mid = 0.0F;
    float m_sigma = 0.0F;
    std::uint32_t m_quietLeft = 0;
    std::uint32_t m_seed = 1;
    bool m_running = false;
};

} // namespace dmm::services
