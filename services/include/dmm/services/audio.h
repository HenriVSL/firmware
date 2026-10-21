// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tommi Tauriainen

#pragma once

#include "dmm/lib/fixed_vector.h"

#include <cstdint>
#include <span>

namespace dmm::services {

/// One step of a beep: a tone, or silence when @c hz is 0.
struct ToneStep
{
    std::uint16_t hz = 0;
    std::uint16_t ms = 0;
};

/// The volume knob position (0..1000 per mille, -1 = no knob) as a step
/// 0..15: 0 at the bottom of its travel, a middle step without a knob.
int volumeStep(int permille);

/// What a board does for the speaker: the amplifier, the switch that feeds
/// it receive audio, and a tone generator mixed in before the amplifier.
class AudioHardware
{
public:
    virtual void setAmplifier(bool on) = 0;
    virtual void setReceiveRoute(bool on) = 0;
    /// Starts a tone (or changes a running one). @p level 1..15, from quiet
    /// to the loudest beep the board allows.
    virtual void startTone(std::uint16_t hz, int level) = 0;
    /// Ends the tone at the end of its current cycle.
    virtual void stopTone() = 0;
    /// Connects the tone output to its pin (on) or leaves the pin floating
    /// (off). On only from just before a beep's amplifier switch-on to its
    /// end.
    virtual void setToneOutput(bool on) = 0;
    /// Time after switching the amplifier on before sound is clean.
    virtual unsigned amplifierSettleMs() const = 0;

protected:
    ~AudioHardware() = default;
};

/// Who gets the speaker. A beep plays over everything and cuts receive
/// audio while it lasts; the amplifier is off when nobody needs it.
/// Driven by one task: play() and setReceiveAudio() change the wish,
/// update() moves time on.
class Speaker
{
public:
    static constexpr std::size_t MaxSteps = 8;

    explicit Speaker(AudioHardware &hardware);

    /// Plays @p steps at beep level @p level (0 = silent, 1..15), replacing
    /// a beep already playing.
    void play(std::span<const ToneStep> steps, int level, std::uint32_t nowMs);

    /// Receive audio wants the speaker (true) or is done with it.
    void setReceiveAudio(bool on);

    bool beeping() const { return m_beeping; }

    /// Moves time on. Returns how many ms may pass before the next call, or
    /// 0 when nothing is timed.
    std::uint32_t update(std::uint32_t nowMs);

private:
    void apply();
    void startStep(std::uint32_t startMs);
    void finish();

    AudioHardware &m_hardware;
    lib::FixedVector<ToneStep, MaxSteps> m_steps;
    std::size_t m_next = 0;
    std::uint32_t m_stepEnd = 0;
    int m_level = 0;
    bool m_beeping = false;
    bool m_settling = false;
    bool m_receive = false;
    bool m_amplifier = false;
    bool m_route = false;
    bool m_toneOutput = false;
};

} // namespace dmm::services
