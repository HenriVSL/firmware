// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Henri Lavikainen

#include "dmm/services/rtty_loopback.h"

#include <cmath>

namespace dmm::services {

namespace {

std::uint32_t gcd(std::uint32_t a, std::uint32_t b)
{
    while (b != 0) {
        const std::uint32_t t = a % b;
        a = b;
        b = t;
    }
    return a;
}

} // namespace

void RttyLoopback::start(std::span<const std::uint8_t> codes, const RttyModulator::Config &config, std::uint16_t leadMs,
    std::uint16_t tailMs, std::uint32_t modulatorRateHz, std::uint8_t peak, float snrDb, std::uint16_t noiseFirstMs,
    std::uint32_t seed, std::uint32_t outputRateHz)
{
    m_running = false;
    if (modulatorRateHz < outputRateHz || outputRateHz == 0 || peak == 0) {
        return;
    }
    m_modulator.start(codes, config, leadMs, tailMs, modulatorRateHz, peak);
    const std::uint32_t g = gcd(modulatorRateHz, outputRateHz);
    m_inUnits = outputRateHz / g;
    m_outUnits = modulatorRateHz / g;
    m_sum = 0.0F;
    m_carry = 0.0F;
    m_carryUnits = 0;
    // The wave runs from 0 to the peak level.
    m_mid = 0.5F * static_cast<float>(peak);
    m_scale = static_cast<float>(ToneMv) / m_mid;
    // A tone of ToneMv peak has ToneMv^2 / 2 of power; the noise's spread
    // over the whole band up to half the output rate.
    const float tonePower = 0.5F * static_cast<float>(ToneMv) * static_cast<float>(ToneMv);
    const float band = 0.5F * static_cast<float>(outputRateHz) / 3000.0F;
    m_sigma = snrDb >= 99.0F ? 0.0F : std::sqrt(tonePower * band / std::pow(10.0F, snrDb / 10.0F));
    m_quietLeft = static_cast<std::uint32_t>(noiseFirstMs) * outputRateHz / 1000;
    m_seed = seed | 1u;
    m_running = true;
}

float RttyLoopback::noise()
{
    if (m_sigma == 0.0F) {
        return 0.0F;
    }
    // The sum of four uniform values is near enough a normal one.
    float sum = 0.0F;
    for (int i = 0; i < 4; ++i) {
        m_seed ^= m_seed << 13;
        m_seed ^= m_seed >> 17;
        m_seed ^= m_seed << 5;
        sum += static_cast<float>(m_seed >> 8) * (1.0F / 16777216.0F) - 0.5F;
    }
    // Four uniform values on -0.5..0.5 spread by 1/sqrt(3).
    return sum * 1.7320508F * m_sigma;
}

std::size_t RttyLoopback::read(std::span<std::uint16_t> out)
{
    std::size_t n = 0;
    auto put = [&](float mv) {
        const float v = mv + static_cast<float>(DcMv) + noise() + 0.5F;
        out[n++] = static_cast<std::uint16_t>(v < 0.0F ? 0.0F : v > 3300.0F ? 3300.0F
                                                                            : v);
    };
    while (n < out.size() && m_running) {
        if (m_quietLeft > 0) {
            --m_quietLeft;
            put(0.0F);
            continue;
        }
        // The part of the last input sample past the previous output.
        float sum = m_carry;
        std::uint32_t units = m_carryUnits;
        m_carry = 0.0F;
        m_carryUnits = 0;
        while (units < m_outUnits) {
            if (!m_modulator.running()) {
                m_running = false;
                break;
            }
            const float v = static_cast<float>(m_modulator.next()) - m_mid;
            const std::uint32_t take = units + m_inUnits <= m_outUnits ? m_inUnits : m_outUnits - units;
            sum += v * static_cast<float>(take);
            units += take;
            if (take < m_inUnits) {
                m_carry = v * static_cast<float>(m_inUnits - take);
                m_carryUnits = m_inUnits - take;
            }
        }
        if (!m_running) {
            break;
        }
        put(sum / static_cast<float>(m_outUnits) * m_scale);
    }
    return n;
}

} // namespace dmm::services
