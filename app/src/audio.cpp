// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tommi Tauriainen

#include "audio.h"

#include "radio.h"

#include "dmm/lib/variant.h"
#include "dmm/os/os.h"
#include "dmm/services/log.h"

namespace dmm::app::audio {

namespace {

namespace log = services::log;
using services::ToneStep;

services::AudioHardware *hardware = nullptr;
os::Task<320> audioTask;
os::Queue<Message, 8> inbox;

constexpr ToneStep KeyBeep[] = {{800, 80}};
constexpr ToneStep RefusedBeep[] = {{400, 150}};
constexpr ToneStep LockedBeep[] = {{1000, 60}, {0, 40}, {700, 100}};
constexpr ToneStep UnlockedBeep[] = {{700, 60}, {0, 40}, {1000, 100}};
constexpr os::Ticks IdleWaitMs = 1000;

std::span<const ToneStep> steps(Sound sound)
{
    switch (sound) {
    case Sound::Key:
        return KeyBeep;
    case Sound::Refused:
        return RefusedBeep;
    case Sound::Locked:
        return LockedBeep;
    case Sound::Unlocked:
        return UnlockedBeep;
    }
    return {};
}

void audioMain(void *)
{
    static services::Speaker speaker{*hardware};
    int volume = -1;
    while (!os::stopRequested()) {
        const std::uint32_t wait = speaker.update(os::nowMs());
        Message message;
        if (!inbox.receive(message, wait == 0 ? IdleWaitMs : wait)) {
            continue;
        }
        lib::visit(lib::Overload{
                       [&](const Beep &b) { speaker.play(steps(b.sound), volume, os::nowMs()); },
                       [&](const Tone &t) {
                           const ToneStep step{t.hz, t.ms};
                           speaker.play({&step, 1}, volume > 0 ? volume : 1, os::nowMs());
                           log::info("audio") << "tone " << t.hz << " Hz, " << t.ms << " ms at level " << (volume > 0 ? volume : 1);
                       },
                       [&](const VolumeKnob &k) {
                           const int step = services::volumeStep(k.permille);
                           if (step != volume) {
                               volume = step;
                               log::info("audio") << "volume " << step;
                               radio::post(radio::SpeakerVolume{static_cast<std::uint8_t>(step)});
                           }
                       },
                       [&](const ReceiveAudio &r) {
                           speaker.setReceiveAudio(r.on);
                           log::info("audio") << "receive audio " << (r.on ? "on" : "off");
                       },
                   },
            message);
    }
}

} // namespace

void start(services::AudioHardware &h)
{
    hardware = &h;
    audioTask.start("audio", 6, audioMain, nullptr);
}

bool post(const Message &message)
{
    return hardware != nullptr && inbox.send(message);
}

} // namespace dmm::app::audio
