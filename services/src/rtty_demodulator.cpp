// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Henri Lavikainen

#include "dmm/services/rtty_demodulator.h"

#include <algorithm>
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
// The taps as floats with the gain taken out, so a sample needs no
// conversion.
constexpr std::array<float, KernelTaps> scaledKernel()
{
    const std::array<std::uint16_t, KernelTaps> k = makeKernel();
    std::array<float, KernelTaps> out{};
    for (std::size_t i = 0; i < KernelTaps; ++i) {
        out[i] = static_cast<float>(k[i]) / static_cast<float>(D * D * D);
    }
    return out;
}
constexpr std::array<float, KernelTaps> Kernel = scaledKernel();

// The detector's quality to start and to stop passing text on, and the
// time it is averaged over, in bits.
constexpr float SignalOn = 0.65F;
constexpr float SignalOff = 0.5F;
constexpr float SignalBits = 10.0F;
// The tones' level over FastBits falling this far under its peak (which
// sinks over PeakSeconds) ends a signal: the sender stopped.
constexpr float FastBits = 2.0F;
constexpr float PeakSeconds = 4.0F;
constexpr float LevelDrop = 0.03F;
// The power between the tones must stay under this share of a tone's.
constexpr float CentreShare = 0.3F;
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

float RttyDemodulator::Tone::power(const Complex &z, std::size_t at, std::size_t length, float scale)
{
    const Complex r = rotor;
    const Complex turned{z.i * r.i - z.q * r.q, z.i * r.q + z.q * r.i};
    Complex next{r.i * step.i - r.q * step.q, r.i * step.q + r.q * step.i};
    // Kept on the unit circle.
    const float g = 1.5F - 0.5F * (next.i * next.i + next.q * next.q);
    rotor = {next.i * g, next.q * g};
    // The sum over the last bit moves on by one sample; each time round the
    // history it is summed afresh, so rounding cannot build up.
    const std::size_t oldest = at >= length ? at - length : at + MaxBitSamples - length;
    sum.i += turned.i - history[oldest].i;
    sum.q += turned.q - history[oldest].q;
    history[at] = turned;
    if (at == 0) {
        sum = {};
        std::size_t k = MaxBitSamples - length + 1;
        for (std::size_t n = 0; n < length; ++n, ++k) {
            const Complex &h = history[k == MaxBitSamples ? 0 : k];
            sum.i += h.i;
            sum.q += h.q;
        }
    }
    return (sum.i * sum.i + sum.q * sum.q) * scale;
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
    m_centre.step = {1.0F, 0.0F};
    m_referenceGain = noiseGain(half) / noiseGain(beyond);
    m_centreGain = noiseGain(half) / noiseGain(0.0F);
    m_attack = TrackAttack;
    m_decay = 1.0F / (TrackBits * m_bitSamples);
    m_toneSmoothing = 1.0F / (SignalBits * m_bitSamples);
    m_afcSmoothing = 1.0F / (AfcBits * m_bitSamples);
    m_powerScale = 1.0F / static_cast<float>(m_bitLength * m_bitLength);
    m_fastSmoothing = 1.0F / (FastBits * m_bitSamples);
    m_peakDecay = 1.0F / (PeakSeconds * static_cast<float>(BasebandHz));
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
    for (Tone *t : {&m_mark, &m_space, &m_below, &m_above, &m_centre}) {
        t->rotor = {1.0F, 0.0F};
        t->history = {};
        t->sum = {};
        t->peak = t->floor = 0.0F;
    }
    m_historyAt = 0;
    m_tonePower = 0.0F;
    m_toneFast = 0.0F;
    m_tonePeak = 0.0F;
    m_centrePower = 0.0F;
    m_referencePower = 0.0F;
    m_proven = false;
    m_heldCount = 0;
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
    const float q = 1.0F - m_referencePower / m_tonePower;
    return q > 0.0F ? q : 0.0F;
}

void RttyDemodulator::process(std::span<const std::uint16_t> mv, Sink onChar)
{
    run(mv, onChar);
}

void RttyDemodulator::process(std::span<const float> samples, Sink onChar)
{
    run(samples, onChar);
}

template <typename T>
void RttyDemodulator::run(std::span<const T> in, Sink &onChar)
{
    if (in.empty()) {
        return;
    }
    if (!m_dcKnown) {
        m_dc = static_cast<float>(in[0]);
        m_dcKnown = true;
    }
    // The state of the front end in locals for the loop.
    float dc = m_dc;
    Complex mixer = m_mixer;
    Complex step = m_mixerStep;
    Complex s0 = m_sums[0];
    Complex s1 = m_sums[1];
    Complex s2 = m_sums[2];
    std::uint32_t p = m_phase;
    std::size_t counted = 0;
    for (std::size_t n = 0; n < in.size(); ++n) {
        float x = static_cast<float>(in[n]);
        dc += (x - dc) * DcStep;
        x -= dc;
        const Complex mixed{x * mixer.i, x * mixer.q};
        mixer = {mixer.i * step.i - mixer.q * step.q, mixer.i * step.q + mixer.q * step.i};
        // This input is p samples before the next output, so it adds to it
        // and to the two after it with the taps that far apart.
        const float h0 = Kernel[p];
        const float h1 = Kernel[p + D];
        s0.i += h0 * mixed.i;
        s0.q += h0 * mixed.q;
        s1.i += h1 * mixed.i;
        s1.q += h1 * mixed.q;
        if (p + 2 * D < KernelTaps) {
            const float h2 = Kernel[p + 2 * D];
            s2.i += h2 * mixed.i;
            s2.q += h2 * mixed.q;
        }
        if (p != 0) {
            --p;
            continue;
        }
        p = D - 1;
        const Complex z = s0;
        s0 = s1;
        s1 = s2;
        s2 = {};
        const float g = 1.5F - 0.5F * (mixer.i * mixer.i + mixer.q * mixer.q);
        mixer = {mixer.i * g, mixer.q * g};
        m_mixer = mixer;
        m_stats.samples += static_cast<std::uint32_t>(n + 1 - counted);
        counted = n + 1;
        baseband(z, onChar);
        // The AFC may have moved the mixer.
        step = m_mixerStep;
    }
    m_stats.samples += static_cast<std::uint32_t>(in.size() - counted);
    m_dc = dc;
    m_mixer = mixer;
    m_sums = {s0, s1, s2};
    m_phase = p;
}

void RttyDemodulator::baseband(Complex z, Sink &onChar)
{
    m_historyAt = m_historyAt + 1 == MaxBitSamples ? 0 : m_historyAt + 1;
    const std::size_t at = m_historyAt;
    const float markPower = m_mark.power(z, at, m_bitLength, m_powerScale);
    const float spacePower = m_space.power(z, at, m_bitLength, m_powerScale);
    const float belowPower = m_below.power(z, at, m_bitLength, m_powerScale);
    const float abovePower = m_above.power(z, at, m_bitLength, m_powerScale);
    const float centrePower = m_centre.power(z, at, m_bitLength, m_powerScale);
    // The slicer works on the tones' levels, the detector on powers.
    const float mark = std::sqrt(markPower);
    const float space = std::sqrt(spacePower);
    m_tonePower += (markPower + spacePower - m_tonePower) * m_toneSmoothing;
    m_toneFast += (markPower + spacePower - m_toneFast) * m_fastSmoothing;
    const float sunk = m_tonePeak - m_tonePeak * m_peakDecay;
    m_tonePeak = m_toneFast > sunk ? m_toneFast : sunk;
    // Three references weighed as the two tone filters for noise.
    const float beside = (belowPower + abovePower) * m_referenceGain;
    const float reference = (beside + centrePower * m_centreGain) * (2.0F / 3.0F);
    m_referencePower += (reference - m_referencePower) * m_toneSmoothing;
    m_centrePower += (centrePower - m_centrePower) * m_toneSmoothing;
    m_nearPower += (z.i * z.i + z.q * z.q - m_nearPower) * m_afcSmoothing;
    m_besidePower += (beside - m_besidePower) * m_afcSmoothing;
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
            const float weight = std::min(power / m_nearPower, 2.0F);
            m_afcHz = std::clamp(m_afcHz + error * weight * AfcStep, -MaxAfcHz, MaxAfcHz);
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
    // The tones gone quiet end a signal at once, whatever the slower ratio
    // says.
    const bool present = m_toneFast > LevelDrop * m_tonePeak;
    // Between the tones an RTTY signal has little; packet audio or a single
    // tone there has much.
    const bool apart = m_centrePower * m_centreGain < CentreShare * 0.5F * m_tonePower;
    m_signal = present && apart && (m_signal ? q >= SignalOff : q > SignalOn);
    if (m_signal && !was) {
        m_ita2.reset();
    }
    if (!m_signal && was) {
        m_proven = false;
        m_stats.unsignalled += static_cast<std::uint32_t>(m_heldCount);
        m_heldCount = 0;
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
                // Before the signal has proved itself, the characters held
                // back were noise too.
                if (!m_proven) {
                    m_stats.unsignalled += static_cast<std::uint32_t>(m_heldCount);
                    m_heldCount = 0;
                }
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
    // The first characters wait until ProvingFrames framed well in a row.
    if (!m_proven) {
        m_held[m_heldCount++] = code;
        if (m_heldCount < m_held.size()) {
            return;
        }
        m_proven = true;
        for (const std::uint8_t held : m_held) {
            emit(held, onChar);
        }
        m_heldCount = 0;
        return;
    }
    emit(code, onChar);
}

void RttyDemodulator::emit(std::uint8_t code, Sink &onChar)
{
    ++m_stats.characters;
    const char c = m_ita2.decode(code);
    if (c != 0) {
        onChar(c);
    }
}

} // namespace dmm::services
