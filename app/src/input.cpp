// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tommi Tauriainen

#include "input.h"

#include "system.h"
#include "ui.h"

#include "dmm/os/os.h"
#include "dmm/services/log.h"

namespace dmm::app::input {

namespace {

using namespace services;
namespace log = services::log;

os::Task<384> inputTask;
InputDevices *devices = nullptr;

constexpr os::Ticks StepMs = 10;
constexpr os::Ticks VolumeEveryMs = 50;
/// Volume changes smaller than this (in 0..1000) are noise.
constexpr int VolumeStep = 5;

const char *eventName(KeyDebouncer::EventType type)
{
    switch (type) {
    case KeyDebouncer::EventType::Press:
        return "pressed";
    case KeyDebouncer::EventType::Release:
        return "released";
    case KeyDebouncer::EventType::LongPress:
        return "held";
    case KeyDebouncer::EventType::Repeat:
        return "repeat";
    }
    return "?";
}

// Turns raw encoder counts into clicks, keeping the remainder.
class Clicks
{
public:
    void reset(std::int32_t position) { m_base = position; }

    /// Clicks moved since the last call.
    int update(std::int32_t position, int perDetent)
    {
        int clicks = 0;
        while (position - m_base >= perDetent) {
            m_base += perDetent;
            ++clicks;
        }
        while (m_base - position >= perDetent) {
            m_base -= perDetent;
            --clicks;
        }
        return clicks;
    }

private:
    std::int32_t m_base = 0;
};

void inputMain(void *)
{
    InputDevices &dev = *devices;
    KeyDebouncer debouncer;
    Clicks clicks;
    const int perDetent = dev.encoderCountsPerDetent() > 0 ? dev.encoderCountsPerDetent() : 1;
    std::int32_t lastRaw = dev.encoderPosition();
    clicks.reset(lastRaw);
    std::int32_t totalClicks = 0;
    int lastVolume = -2;
    os::Ticks sinceVolume = VolumeEveryMs;

    os::Ticks wake = os::nowMs();
    while (!os::stopRequested()) {
        const KeyMask before = debouncer.pressed();
        debouncer.update(dev.readKeys(), os::nowMs(), [](const KeyDebouncer::Event &e) {
            if (e.type != KeyDebouncer::EventType::Repeat) {
                log::info("input") << "key " << keyName(e.key) << " " << eventName(e.type);
            }
            ui::post(ui::KeyEvent{e.key, e.type});
        });
        if (debouncer.pressed() != before) {
            system::noteActivity();
            ui::post(ui::Keys{debouncer.pressed()});
        }

        const std::int32_t raw = dev.encoderPosition();
        if (raw != lastRaw) {
            lastRaw = raw;
            ui::post(ui::EncoderRaw{raw});
            const int moved = clicks.update(raw, perDetent);
            if (moved != 0) {
                system::noteActivity();
                totalClicks += moved;
                log::info("input") << "knob " << (moved > 0 ? "+" : "") << moved << " (" << totalClicks << ")";
                ui::post(ui::EncoderClicks{totalClicks});
            }
        }

        sinceVolume += StepMs;
        if (sinceVolume >= VolumeEveryMs) {
            sinceVolume = 0;
            const int volume = dev.volumeKnob();
            const int change = volume > lastVolume ? volume - lastVolume : lastVolume - volume;
            if (lastVolume == -2 || change >= VolumeStep || (volume < 0) != (lastVolume < 0)) {
                lastVolume = volume;
                ui::post(ui::Volume{static_cast<std::int16_t>(volume)});
            }
        }

        os::sleepUntil(wake, StepMs);
    }
}

} // namespace

void start(InputDevices &d)
{
    devices = &d;
    inputTask.start("input", 5, inputMain, nullptr);
}

} // namespace dmm::app::input
