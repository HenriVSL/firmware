// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tommi Tauriainen

#include "dmm/services/audio.h"

#include <doctest/doctest.h>

#include <string>
#include <vector>

using dmm::services::Speaker;
using dmm::services::ToneStep;
using dmm::services::volumeStep;

namespace {

class FakeAudio final : public dmm::services::AudioHardware
{
public:
    void setAmplifier(bool on) override
    {
        amplifier = on;
        events.push_back(on ? "amp on" : "amp off");
    }
    void setReceiveRoute(bool on) override
    {
        route = on;
        events.push_back(on ? "route on" : "route off");
    }
    void startTone(std::uint16_t hz, int level) override
    {
        tone = hz;
        events.push_back("tone " + std::to_string(hz) + " L" + std::to_string(level));
    }
    void stopTone() override
    {
        tone = 0;
        events.push_back("stop");
    }
    void setToneOutput(bool on) override
    {
        toneOutput = on;
        events.push_back(on ? "pin on" : "pin off");
    }
    unsigned amplifierSettleMs() const override { return 50; }

    bool amplifier = false;
    bool route = false;
    std::uint16_t tone = 0;
    bool toneOutput = false;
    std::vector<std::string> events;
};

constexpr ToneStep KeyBeep[] = {{800, 80}};

} // namespace

TEST_CASE("the knob maps to volume steps 0..15")
{
    CHECK(volumeStep(0) == 0);
    CHECK(volumeStep(29) == 0);
    CHECK(volumeStep(30) == 1);
    CHECK(volumeStep(1000) == 15);
    CHECK(volumeStep(-1) == 8);
    int previous = 0;
    for (int p = 0; p <= 1000; ++p) {
        const int s = volumeStep(p);
        CHECK(s >= previous);
        CHECK(s - previous <= 1);
        previous = s;
    }
}

TEST_CASE("a beep switches the amplifier on, waits for it, plays, then switches it off")
{
    FakeAudio hw;
    Speaker spk(hw);
    spk.play(KeyBeep, 5, 1000);
    CHECK(hw.amplifier);
    CHECK(hw.tone == 0);
    CHECK(spk.update(1000) == 50);
    CHECK(spk.update(1049) == 1);
    CHECK(spk.update(1050) == 80);
    CHECK(hw.tone == 800);
    CHECK(spk.update(1130) == 20); // tail
    CHECK(hw.tone == 0);
    CHECK(hw.amplifier);
    CHECK(spk.update(1150) == 0);
    CHECK_FALSE(spk.beeping());
    CHECK_FALSE(hw.amplifier);
    CHECK(hw.events == std::vector<std::string>{"pin on", "amp on", "tone 800 L5", "stop", "stop", "pin off", "amp off"});
}

TEST_CASE("a beep cuts receive audio and gives it back, without a settle wait")
{
    FakeAudio hw;
    Speaker spk(hw);
    spk.setReceiveAudio(true);
    CHECK(hw.route);
    CHECK(hw.amplifier);
    hw.events.clear();
    spk.play(KeyBeep, 15, 0);
    CHECK_FALSE(hw.route);
    CHECK(hw.tone == 800);
    spk.update(80);
    spk.update(100);
    CHECK(hw.route);
    CHECK(hw.amplifier);
    CHECK(hw.events == std::vector<std::string>{"pin on", "route off", "tone 800 L15", "stop", "stop", "pin off", "route on"});
    spk.setReceiveAudio(false);
    CHECK_FALSE(hw.amplifier);
    CHECK_FALSE(hw.route);
}

TEST_CASE("level 0 is silent, a new beep replaces the old one, steps and pitch changes follow")
{
    FakeAudio hw;
    Speaker spk(hw);
    spk.play(KeyBeep, 0, 0);
    CHECK_FALSE(spk.beeping());
    CHECK(hw.events.empty());

    constexpr ToneStep TwoTone[] = {{1400, 100}, {1000, 100}};
    spk.play(KeyBeep, 3, 0);
    spk.update(50);
    CHECK(hw.tone == 800);
    spk.play(TwoTone, 3, 60);
    CHECK(hw.tone == 1400);
    spk.update(160);
    CHECK(hw.tone == 1000);
    spk.update(260);
    spk.update(280);
    CHECK_FALSE(spk.beeping());
    CHECK_FALSE(hw.amplifier);
    CHECK_FALSE(hw.toneOutput);
}

TEST_CASE("the tone pin floats whenever no beep plays")
{
    FakeAudio hw;
    Speaker spk(hw);
    spk.setReceiveAudio(true);
    CHECK_FALSE(hw.toneOutput);
    spk.play(KeyBeep, 4, 0);
    CHECK(hw.toneOutput);
    spk.update(80);
    CHECK(hw.toneOutput); // tail: the last cycle may still be playing
    spk.update(100);
    CHECK_FALSE(hw.toneOutput);
    CHECK(hw.route);
    spk.play(KeyBeep, 0, 200);
    CHECK_FALSE(hw.toneOutput);
}

TEST_CASE("a late update skips over steps that have ended")
{
    FakeAudio hw;
    Speaker spk(hw);
    constexpr ToneStep Three[] = {{500, 10}, {600, 10}, {700, 10}};
    spk.play(Three, 1, 0);
    CHECK(spk.update(10'000) == 0);
    CHECK_FALSE(spk.beeping());
    CHECK_FALSE(hw.amplifier);
}
