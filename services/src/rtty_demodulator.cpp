// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Henri Lavikainen

#include "dmm/services/rtty_demodulator.h"

#include <cmath>

namespace dmm::services {

namespace {

constexpr float Pi = 3.14159265F;
constexpr std::uint32_t D = RttyDemodulator::Decimation;

// Three moving averages of D samples in a row, as one filter: its taps are
// whole numbers adding up to D cubed.
constexpr std::size_t KernelTaps = 3 * (D - 1) + 1;
constexpr std::array<std::uint16_t, KernelTaps> makeKernel()
{
    std::array<std::uint32_t, KernelTaps> a{};
    for (std::size_t i = 0; i < D; ++i) {
        a[i] = 1;
    }
    for (int pass = 0; pass < 2; ++pass) {
        std::array<std::uint32_t, KernelTaps> b{};
        for (std::size_t i = 0; i < KernelTaps; ++i) {
            for (std::size_t j = 0; j < D && i + j < KernelTaps; ++j) {
                b[i + j] += a[i];
            }
        }
        a = b;
    }
    std::array<std::uint16_t, KernelTaps> out{};
    for (std::size_t i = 0; i < KernelTaps; ++i) {
        out[i] = static_cast<std::uint16_t>(a[i]);
    }
    return out;
}
constexpr std::array<std::uint16_t, KernelTaps> Kernel = makeKernel();
constexpr float KernelScale = 1.0F / static_cast<float>(D * D * D);

// The detector's quality to start and to stop passing text on, and the
// time it is averaged over, in bits.
constexpr float SignalOn = 0.65F;
constexpr float SignalOff = 0.5F;
constexpr float SignalBits = 10.0F;
// The noise references sit this far beyond the tones, at most this close
// to half the 600 Hz rate.
constexpr float ReferenceBeyondHz = 170.0F;
constexpr float ReferenceMaxHz = 270.0F;
// The AFC follows while the audio near the tones has this many times the
// power beside them; each step moves by this share of the difference; it
// takes frequencies this far from the middle (not those of a change from
// one tone to the other); and drifts back after this long without.
constexpr float AfcNear = 8.0F;
constexpr float AfcStep = 1.0F / 300.0F;
constexpr float AfcLowHz = 30.0F;
constexpr float AfcHighHz = 170.0F;
constexpr float AfcIdleSeconds = 2.0F;
constexpr float AfcReturnSeconds = 3.0F;
// The AFC's power estimates follow over this many bits.
constexpr float AfcBits = 3.0F;
// How fast each tone's peak and floor follow its level: a jump towards
// the level at once, a return over this many bits.
constexpr float TrackAttack = 0.25F;
constexpr float TrackBits = 8.0F;
// A start bit counts only after this much of a bit of mark.
constexpr float MarkBeforeStart = 0.4F;
// The DC follower's step.
constexpr float DcStep = 1.0F / 512.0F;

// A unit phasor turning by -@p hz at @p rateHz.
auto turn(float hz, float rateHz)
{
    struct C
    {
        float i;
        float q;
    };
    const float w = 2.0F * Pi * hz / rateHz;
    return C{std::cos(w), -std::sin(w)};
}

// The decimating filter's gain at @p hz from the mixing frequency.
float kernelGain(float hz)
{
    const float x = Pi * hz / static_cast<float>(RttyDemodulator::SampleRateHz);
    if (std::fabs(x) < 1e-6F) {
        return 1.0F;
    }
    const float one = std::sin(static_cast<float>(D) * x) / (static_cast<float>(D) * std::sin(x));
    return one * one * one;
}

// The decimated noise power a filter at @p hz gets from white noise: from
// @p hz and from its image half the 600 Hz rate further.
float noiseGain(float hz)
{
    const float a = kernelGain(hz);
    const float b = kernelGain(static_cast<float>(RttyDemodulator::BasebandHz) - std::fabs(hz));
    return a * a + b * b;
}

} // namespace

float RttyDemodulator::Tone::level(const Complex &z, std::size_t at, std::size_t length)
{
    const Complex r = rotor;
    history[at] = {z.i * r.i - z.q * r.q, z.i * r.q + z.q * r.i};
    Complex next{r.i * step.i - r.q * step.q, r.i * step.q + r.q * step.i};
    // Kept on the unit circle.
    const float g = 1.5F - 0.5F * (next.i * next.i + next.q * next.q);
    rotor = {next.i * g, next.q * g};
    float si = 0.0F;
    float sq = 0.0F;
    for (std::size_t k = 0; k < length; ++k) {
        const Complex &h = history[(at + MaxBitSamples - k) % MaxBitSamples];
        si += h.i;
        sq += h.q;
    }
    return std::sqrt(si * si + sq * sq) / static_cast<float>(length);
}

void RttyDemodulator::Tone::track(float level, bool on, float attack, float decay)
{
    // The peak follows the level while the tone is sent, the floor while
    // the other one is: a long run of one tone keeps the other's range.
    if (on) {
        peak += (level - peak) * (level > peak ? attack : decay);
    } else {
        floor += (level - floor) * (level < floor ? attack : decay);
    }
}

void RttyDemodulator::configure(const Config &config)
{
    m_config = config;
    m_ita2.setFigures(config.figures);
    m_ita2.setUnshiftOnSpace(config.unshiftOnSpace);
    const float baud = config.baud >= 40.0F && config.baud <= 300.0F ? config.baud : 45.45F;
    m_bitSamples = static_cast<float>(BasebandHz) / baud;
    m_bitLength = static_cast<std::size_t>(m_bitSamples + 0.5F);
    if (m_bitLength > MaxBitSamples) {
        m_bitLength = MaxBitSamples;
    }
    if (m_bitLength < 2) {
        m_bitLength = 2;
    }
    const float half = 0.5F * static_cast<float>(config.shiftHz);
    m_centreHz = static_cast<float>(config.markHz) + half;
    m_toneStepHz = half;
    m_afcHz = 0.0F;
    setMixer();
    const float markOffset = config.reverse ? half : -half;
    const auto m = turn(markOffset, static_cast<float>(BasebandHz));
    const auto s = turn(-markOffset, static_cast<float>(BasebandHz));
    m_mark.step = {m.i, m.q};
    m_space.step = {s.i, s.q};
    const float beyond = std::fmin(half + ReferenceBeyondHz, ReferenceMaxHz);
    const auto below = turn(-beyond, static_cast<float>(BasebandHz));
    const auto above = turn(beyond, static_cast<float>(BasebandHz));
    m_below.step = {below.i, below.q};
    m_above.step = {above.i, above.q};
    m_referenceGain = noiseGain(half) / noiseGain(beyond);
    m_attack = TrackAttack;
    m_decay = 1.0F / (TrackBits * m_bitSamples);
    m_toneSmoothing = 1.0F / (SignalBits * m_bitSamples);
    reset();
}

void RttyDemodulator::setMixer()
{
    const auto mix = turn(m_centreHz + m_afcHz, static_cast<float>(SampleRateHz));
    m_mixerStep = {mix.i, mix.q};
    m_afcSet = m_afcHz;
}

void RttyDemodulator::reset()
{
    m_dcKnown = false;
    m_mixer = {1.0F, 0.0F};
    m_sums = {};
    m_phase = D - 1;
    for (Tone *t : {&m_mark, &m_space, &m_below, &m_above}) {
        t->rotor = {1.0F, 0.0F};
        t->history = {};
        t->peak = t->floor = 0.0F;
    }
    m_historyAt = 0;
    m_tonePower = 0.0F;
    m_referencePower = 0.0F;
    m_lastZ = {};
    m_nearPower = 0.0F;
    m_besidePower = 0.0F;
    m_afcIdle = 0;
    m_signal = false;
    m_frame = Frame::Hunting;
    m_last = 0.0F;
    m_markRun = 0.0F;
    m_ita2.reset();
}

float RttyDemodulator::quality() const
{
    if (m_tonePower <= 0.0F) {
        return 0.0F;
    }
    const float q = 1.0F - m_referencePower * m_referenceGain / m_tonePower;
    return q > 0.0F ? q : 0.0F;
}

void RttyDemodulator::process(std::span<const std::uint16_t> mv, Sink onChar)
{
    for (const std::uint16_t v : mv) {
        sample(static_cast<float>(v), onChar);
    }
}

void RttyDemodulator::process(std::span<const float> samples, Sink onChar)
{
    for (const float v : samples) {
        sample(v, onChar);
    }
}

void RttyDemodulator::sample(float x, Sink &onChar)
{
    ++m_stats.samples;
    if (!m_dcKnown) {
        m_dc = x;
        m_dcKnown = true;
    }
    m_dc += (x - m_dc) * DcStep;
    x -= m_dc;

    const Complex mixed{x * m_mixer.i, x * m_mixer.q};
    m_mixer = {m_mixer.i * m_mixerStep.i - m_mixer.q * m_mixerStep.q,
        m_mixer.i * m_mixerStep.q + m_mixer.q * m_mixerStep.i};
    // This input is m_phase samples before the next output, so it adds to
    // it and to the two after it with the taps that far apart.
    const std::uint32_t p = m_phase;
    for (std::uint32_t k = 0; k < 3; ++k) {
        const std::uint32_t tap = p + k * D;
        if (tap < KernelTaps) {
            const float h = static_cast<float>(Kernel[tap]);
            m_sums[k].i += h * mixed.i;
            m_sums[k].q += h * mixed.q;
        }
    }
    if (p != 0) {
        --m_phase;
        return;
    }
    m_phase = D - 1;
    const Complex z{m_sums[0].i * KernelScale, m_sums[0].q * KernelScale};
    m_sums[0] = m_sums[1];
    m_sums[1] = m_sums[2];
    m_sums[2] = {};
    const float g = 1.5F - 0.5F * (m_mixer.i * m_mixer.i + m_mixer.q * m_mixer.q);
    m_mixer = {m_mixer.i * g, m_mixer.q * g};
    baseband(z, onChar);
}

void RttyDemodulator::baseband(Complex z, Sink &onChar)
{
    m_historyAt = (m_historyAt + 1) % MaxBitSamples;
    const float mark = m_mark.level(z, m_historyAt, m_bitLength);
    const float space = m_space.level(z, m_historyAt, m_bitLength);
    const float below = m_below.level(z, m_historyAt, m_bitLength);
    const float above = m_above.level(z, m_historyAt, m_bitLength);
    m_tonePower += (mark * mark + space * space - m_tonePower) * m_toneSmoothing;
    m_referencePower += (below * below + above * above - m_referencePower) * m_toneSmoothing;
    const float afcSmoothing = 1.0F / (AfcBits * m_bitSamples);
    m_nearPower += (z.i * z.i + z.q * z.q - m_nearPower) * afcSmoothing;
    m_besidePower += ((below * below + above * above) * m_referenceGain - m_besidePower) * afcSmoothing;
    followTones(z);
    updateSignal();
    const bool markNow = m_last > 0.0F;
    m_mark.track(mark, markNow, m_attack, m_decay);
    m_space.track(space, !markNow, m_attack, m_decay);
    decide((mark - m_mark.middle()) - (space - m_space.middle()), onChar);
}

void RttyDemodulator::followTones(Complex z)
{
    // The phase step from the last sample, as a frequency from the mixer.
    const Complex p{z.i * m_lastZ.i + z.q * m_lastZ.q, z.q * m_lastZ.i - z.i * m_lastZ.q};
    m_lastZ = z;
    const float power = z.i * z.i + z.q * z.q;
    const bool near = m_nearPower > AfcNear * m_besidePower && m_nearPower > 0.0F;
    if (near) {
        m_afcIdle = 0;
        const float hz = std::atan2(p.q, p.i) * static_cast<float>(BasebandHz) / (2.0F * Pi);
        const float away = std::fabs(hz);
        if (away > AfcLowHz && away < AfcHighHz) {
            const float error = hz - (hz > 0.0F ? m_toneStepHz : -m_toneStepHz);
            // Louder samples count more, a click no more than twice.
            const float weight = std::fmin(power / m_nearPower, 2.0F);
            m_afcHz += error * weight * AfcStep;
            m_afcHz = std::fmax(-MaxAfcHz, std::fmin(MaxAfcHz, m_afcHz));
        }
    } else if (static_cast<float>(++m_afcIdle) > AfcIdleSeconds * static_cast<float>(BasebandHz)) {
        m_afcIdle = static_cast<std::uint32_t>(AfcIdleSeconds * static_cast<float>(BasebandHz));
        m_afcHz -= m_afcHz / (AfcReturnSeconds * static_cast<float>(BasebandHz));
    }
    if (std::fabs(m_afcHz - m_afcSet) > 0.25F) {
        setMixer();
    }
}

void RttyDemodulator::updateSignal()
{
    const float q = quality();
    const bool was = m_signal;
    m_signal = m_signal ? q >= SignalOff : q > SignalOn;
    if (m_signal && !was) {
        m_ita2.reset();
    }
    if (m_signal) {
        m_stats.signalSamples += D;
    }
}

void RttyDemodulator::decide(float v, Sink &onChar)
{
    const float runBefore = m_markRun;
    m_markRun = v > 0.0F ? m_markRun + 1.0F : 0.0F;
    if (m_frame == Frame::Hunting) {
        if (m_last > 0.0F && v <= 0.0F && runBefore >= MarkBeforeStart * m_bitSamples) {
            // The change lies between the two samples, where the line
            // through them crosses zero.
            m_sinceStart = 1.0F - m_last / (m_last - v);
            m_bit = 0;
            m_code = 0;
            m_startSignalled = m_signal;
            m_frame = Frame::Reading;
        }
        m_last = v;
        return;
    }
    m_sinceStart += 1.0F;
    const float at = (static_cast<float>(m_bit) + 0.5F) * m_bitSamples;
    if (m_sinceStart >= at) {
        const float u = at - (m_sinceStart - 1.0F);
        const float value = m_last + (v - m_last) * u;
        const bool markBit = value > 0.0F;
        if (m_bit == 0) {
            if (markBit) {
                m_frame = Frame::Hunting; // a glitch, not a start bit
            }
        } else if (m_bit <= 5) {
            if (markBit) {
                m_code = static_cast<std::uint8_t>(m_code | 1u << (m_bit - 1));
            }
        } else {
            if (markBit) {
                character(m_code, onChar);
            } else {
                ++m_stats.framingErrors;
            }
            m_frame = Frame::Hunting;
        }
        ++m_bit;
    }
    m_last = v;
}

void RttyDemodulator::character(std::uint8_t code, Sink &onChar)
{
    // Begun or ended without a signal: noise.
    if (!m_signal || !m_startSignalled) {
        ++m_stats.unsignalled;
        return;
    }
    ++m_stats.characters;
    const char c = m_ita2.decode(code);
    if (c != 0) {
        onChar(c);
    }
}

} // namespace dmm::services
