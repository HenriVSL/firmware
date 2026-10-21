// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tommi Tauriainen

#pragma once

#include "dmm/services/audio.h"

#include <cstdint>
#include <variant>

namespace dmm::app::audio {

enum class Sound : std::uint8_t
{
    Key,
    Refused,  // a locked key
    Locked,   // the keys were locked
    Unlocked, // the keys were unlocked
};

/// One of the radio's own beeps, at the volume knob's level.
struct Beep
{
    Sound sound;
};
/// A test tone (console).
struct Tone
{
    std::uint16_t hz;
    std::uint16_t ms;
};
/// The volume knob moved: 0..1000 per mille, -1 = no knob.
struct VolumeKnob
{
    std::int16_t permille;
};
/// Receive audio wants the speaker, or is done with it.
struct ReceiveAudio
{
    bool on;
};

using Message = std::variant<Beep, Tone, VolumeKnob, ReceiveAudio>;

/// Starts the audio task, which owns the speaker through @p hardware.
void start(services::AudioHardware &hardware);

/// Queues a message for the audio task; false when the queue is full or
/// there is no audio hardware.
bool post(const Message &message);

} // namespace dmm::app::audio
