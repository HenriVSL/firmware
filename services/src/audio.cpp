// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tommi Tauriainen

#include "dmm/services/audio.h"

namespace dmm::services {

namespace {

// Below this the knob counts as turned fully down.
constexpr int KnobOffPermille = 30;
constexpr int MaxStep = 15;
constexpr int NoKnobStep = 8;
// Silence after the last step before the amplifier may switch off, so the
// last cycle of the tone is not cut.
constexpr std::uint16_t TailMs = 20;

bool reached(std::uint32_t nowMs, std::uint32_t at)
{
    return static_cast<std::int32_t>(nowMs - at) >= 0;
}

} // namespace

int volumeStep(int permille)
{
    if (permille < 0) {
        return NoKnobStep;
    }
    if (permille < KnobOffPermille) {
        return 0;
    }
    const int step = 1 + (permille - KnobOffPermille) * MaxStep / (1001 - KnobOffPermille);
    return step > MaxStep ? MaxStep : step;
}

Speaker::Speaker(AudioHardware &hardware)
    : m_hardware(hardware)
{
}

void Speaker::apply()
{
    const bool amplifier = m_beeping || m_receive;
    const bool route = m_receive && !m_beeping;
    // Off: amplifier first, then the route. On: route first, then amplifier.
    if (!amplifier && m_amplifier) {
        m_hardware.setAmplifier(false);
        m_amplifier = false;
    }
    if (route != m_route) {
        m_hardware.setReceiveRoute(route);
        m_route = route;
    }
    if (amplifier && !m_amplifier) {
        m_hardware.setAmplifier(true);
        m_amplifier = true;
    }
}

void Speaker::play(std::span<const ToneStep> steps, int level, std::uint32_t nowMs)
{
    if (m_beeping) {
        m_hardware.stopTone();
    }
    m_steps.clear();
    if (level <= 0 || steps.empty()) {
        finish();
        return;
    }
    for (const ToneStep &s : steps) {
        if (m_steps.size() + 1 >= MaxSteps) {
            break;
        }
        m_steps.pushBack(s);
    }
    m_steps.pushBack(ToneStep{0, TailMs});
    m_level = level > MaxStep ? MaxStep : level;
    m_next = 0;
    const bool settle = !m_amplifier;
    if (!m_toneOutput) {
        m_hardware.setToneOutput(true);
        m_toneOutput = true;
    }
    m_beeping = true;
    apply();
    if (settle) {
        m_settling = true;
        m_stepEnd = nowMs + m_hardware.amplifierSettleMs();
    } else {
        m_settling = false;
        startStep(nowMs);
    }
}

void Speaker::setReceiveAudio(bool on)
{
    m_receive = on;
    apply();
}

void Speaker::finish()
{
    if (m_beeping) {
        m_hardware.stopTone();
    }
    if (m_toneOutput) {
        m_hardware.setToneOutput(false);
        m_toneOutput = false;
    }
    m_beeping = false;
    m_settling = false;
    apply();
}

// Steps are timed from the end of the previous one, so a late update()
// does not stretch the beep.
void Speaker::startStep(std::uint32_t startMs)
{
    if (m_next >= m_steps.size()) {
        finish();
        return;
    }
    const ToneStep &s = m_steps[m_next++];
    if (s.hz != 0) {
        m_hardware.startTone(s.hz, m_level);
    } else {
        m_hardware.stopTone();
    }
    m_stepEnd = startMs + s.ms;
}

std::uint32_t Speaker::update(std::uint32_t nowMs)
{
    while (m_beeping && reached(nowMs, m_stepEnd)) {
        m_settling = false;
        startStep(m_stepEnd);
    }
    if (!m_beeping) {
        return 0;
    }
    const std::uint32_t left = m_stepEnd - nowMs;
    return left == 0 ? 1 : left;
}

} // namespace dmm::services
