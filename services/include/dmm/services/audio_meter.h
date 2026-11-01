// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Henri Lavikainen

#pragma once

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <span>

namespace dmm::services {

/// The board's sampler of the received audio, for measurements; the board
/// names the type (dmm/board/parts.h).
///
/// - start(rateHz): starts sampling in the background; false if the board
///   cannot sample at that rate.
/// - read(out): the samples taken since the last read, oldest first, in
///   mV at the sampling point; returns how many (up to out.size()).
/// - lostSamples(): samples dropped since start because read() came too
///   late.
/// - stop(): stops sampling.
template <class T>
concept AudioSampler = requires(T &s, std::span<std::uint16_t> out) {
    { s.start(std::uint32_t{}) } -> std::same_as<bool>;
    { s.read(out) } -> std::same_as<std::size_t>;
    { s.lostSamples() } -> std::same_as<std::uint32_t>;
    s.stop();
};

/// Level and tone statistics of a stream of audio samples in mV: mean,
/// extremes, the RMS of the part above the mean, and the level of a few
/// tones, each averaged over back-to-back windows.
class AudioMeter
{
public:
    static constexpr std::size_t MaxTones = 4;

    struct Result
    {
        std::uint32_t samples = 0;
        int meanMv = 0;
        int minMv = 0;
        int maxMv = 0;
        /// RMS of the samples minus the mean.
        float acRmsMv = 0.0F;
        /// RMS level of each tone, over the full windows (0 without one).
        float toneRmsMv[MaxTones] = {};
        std::uint32_t windows = 0;
    };

    /// Measures @p tonesHz (up to MaxTones) at @p sampleRateHz over windows
    /// of @p windowSamples. A tone with a whole number of cycles per window
    /// is not seen at all by the others' measurements.
    AudioMeter(std::uint32_t sampleRateHz, std::span<const std::uint32_t> tonesHz, std::uint32_t windowSamples);

    void add(std::uint16_t mv);
    void add(std::span<const std::uint16_t> mv)
    {
        for (std::uint16_t v : mv) {
            add(v);
        }
    }

    Result result() const;

    /// 10 log10(@p a^2 / @p b^2) for two levels, limited to +-99 dB.
    static float ratioDb(float a, float b);

private:
    struct Tone
    {
        float coefficient = 0.0F;
        float s1 = 0.0F;
        float s2 = 0.0F;
        double powerSum = 0.0;
    };

    void endWindow();

    Tone m_tones[MaxTones];
    std::size_t m_toneCount = 0;
    std::uint32_t m_windowSamples;
    std::uint32_t m_inWindow = 0;
    std::uint32_t m_windows = 0;
    std::uint32_t m_count = 0;
    std::uint64_t m_sum = 0;
    std::uint64_t m_sumSquares = 0;
    std::uint16_t m_min = 0xFFFF;
    std::uint16_t m_max = 0;
    // Samples go into the tone filters minus this, to keep their sums small.
    float m_offset = 0.0F;
};

} // namespace dmm::services
