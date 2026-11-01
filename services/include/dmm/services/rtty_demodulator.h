// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Henri Lavikainen

#pragma once

#include "dmm/lib/fn_ref.h"
#include "dmm/services/rtty.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace dmm::services {

/// Receives RTTY (two-tone FSK, asynchronous 5-bit ITA2 characters: one
/// start bit, five data bits, at least one stop bit) from audio sampled at
/// 9600 Hz.
///
/// The audio is mixed down around the middle of the tone pair and low-pass
/// filtered to 600 samples a second (a three-stage moving average). There
/// each tone is turned to 0 Hz and averaged over one bit, which gives the
/// level of each tone. Each tone's level is followed on its own, its
/// recent peak and floor, and a bit is mark when the mark tone lies further
/// above the middle of its own range than the space tone above the middle
/// of its range: a tone louder than the other (tilt), fading or lost
/// altogether moves only its own middle. Characters are framed as by a
/// serial port: a mark-to-space change, placed between samples, starts
/// one; the bits are read at their middles from there, the start bit must
/// still be space and the stop bit mark, else the character is dropped as
/// a framing error.
///
/// A signal detector compares the power in the two tone filters with the
/// power of all the audio below about 3 kHz: text is passed on only while
/// that share stays high, so noise (with the squelch open or not) gives
/// none.
///
/// Heap-free; about 0.3 KB of state.
class RttyDemodulator
{
public:
    static constexpr std::uint32_t SampleRateHz = 9600;
    static constexpr std::uint32_t Decimation = 16;
    static constexpr std::uint32_t BasebandHz = SampleRateHz / Decimation;
    /// The longest bit at 600 samples a second (45.45 Bd and up).
    static constexpr std::size_t MaxBitSamples = 14;

    struct Config
    {
        std::uint16_t markHz = 2125;
        std::uint16_t shiftHz = rtty::ShiftHz;
        float baud = 45.45F;
        /// Mark and space swapped.
        bool reverse = false;
        bool unshiftOnSpace = true;
        rtty::Figures figures = rtty::Figures::UsTty;
    };

    struct Stats
    {
        /// Characters passed on (shifts included), framing errors, and
        /// characters dropped because no signal was detected.
        std::uint32_t characters = 0;
        std::uint32_t framingErrors = 0;
        std::uint32_t unsignalled = 0;
        /// Samples taken in, and of them with a signal detected.
        std::uint32_t samples = 0;
        std::uint32_t signalSamples = 0;
    };

    /// Called with each character passed on (as Ita2Decoder gives them).
    using Sink = lib::FnRef<void(char c)>;

    RttyDemodulator() { configure(Config{}); }

    /// Takes new settings; forgets the audio in progress.
    void configure(const Config &config);
    const Config &config() const { return m_config; }

    /// Forgets the audio and character in progress (after a gap in the
    /// samples); the statistics stay.
    void reset();
    void clearStats() { m_stats = {}; }

    /// Takes samples in mV (any DC level) or as signed levels.
    void process(std::span<const std::uint16_t> mv, Sink onChar);
    void process(std::span<const float> samples, Sink onChar);

    /// The share of the audio's power in the tone filters, about 0.8 for a
    /// clean signal and a few hundredths for noise.
    float quality() const;
    /// A signal is detected: text is passed on.
    bool signal() const { return m_signal; }

    const Stats &stats() const { return m_stats; }

private:
    struct Complex
    {
        float i = 0.0F;
        float q = 0.0F;
    };
    struct Tone
    {
        // Turns the tone to 0 Hz at 600 samples a second.
        Complex rotor{1.0F, 0.0F};
        Complex step{1.0F, 0.0F};
        // The last bit's worth of rotated samples.
        std::array<Complex, MaxBitSamples> history{};
        float peak = 0.0F;
        float floor = 0.0F;

        float level(const Complex &z, std::size_t at, std::size_t length);
        void track(float level, bool on, float attack, float decay);
        float middle() const { return 0.5F * (peak + floor); }
    };

    enum class Frame : std::uint8_t
    {
        Hunting, // waiting for a start bit
        Reading, // bits of a character
    };

    void sample(float x, Sink &onChar);
    void baseband(Complex z, Sink &onChar);
    void decide(float v, Sink &onChar);
    void character(std::uint8_t code, Sink &onChar);
    void updateSignal();

    Config m_config;
    rtty::Ita2Decoder m_ita2;
    float m_dc = 0.0F;
    bool m_dcKnown = false;
    std::array<float, 2> m_previous{};
    // Mixes the audio down around the middle of the tone pair.
    Complex m_mixer{1.0F, 0.0F};
    Complex m_mixerStep{1.0F, 0.0F};
    // The decimating filter's sums for the next three outputs, and the
    // place of the next input in the output period.
    std::array<Complex, 3> m_sums{};
    std::uint32_t m_phase = 0;
    Tone m_mark;
    Tone m_space;
    std::size_t m_historyAt = 0;
    std::size_t m_bitLength = 13;
    float m_bitSamples = 13.2F;
    float m_attack = 0.25F;
    float m_decay = 0.01F;
    // Power of all the audio and of the tones, followed slowly.
    float m_audioPower = 0.0F;
    float m_tonePower = 0.0F;
    float m_audioSmoothing = 0.001F;
    float m_toneSmoothing = 0.01F;
    float m_toneGain = 1.0F;
    bool m_signal = false;
    // Character framing at 600 samples a second.
    Frame m_frame = Frame::Hunting;
    float m_last = 0.0F;
    float m_markRun = 0.0F;
    float m_sinceStart = 0.0F;
    std::uint8_t m_bit = 0;
    std::uint8_t m_code = 0;
    bool m_startSignalled = false;
    Stats m_stats;
};

} // namespace dmm::services
