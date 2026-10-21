// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tommi Tauriainen

#include "dmm/lib/tone_synth.h"
#include "dmm/lib/variant.h"

#include <doctest/doctest.h>

using dmm::lib::ToneSynth;

TEST_CASE("the tone wave is a symmetric raised cosine")
{
    for (std::size_t i = 1; i < ToneSynth::TableSize; ++i) {
        CHECK(ToneSynth::Wave[i] == ToneSynth::Wave[ToneSynth::TableSize - i]);
    }
    CHECK(ToneSynth::Wave[ToneSynth::TableSize / 4] == 128);
}

TEST_CASE("a tone has the asked pitch and peak and starts at zero")
{
    ToneSynth synth;
    CHECK(synth.next() == 0);
    synth.start(1000, 32000, 200);
    int cycles = 0;
    int highest = 0;
    int previous = 0;
    bool rising = true;
    const int first = synth.next();
    CHECK(first < 10);
    for (int i = 1; i < 32000; ++i) {
        const int v = synth.next();
        highest = v > highest ? v : highest;
        if (rising && v < previous) {
            rising = false;
        } else if (!rising && v > previous) {
            rising = true;
            ++cycles;
        }
        previous = v;
    }
    CHECK(cycles >= 999);
    CHECK(cycles <= 1000);
    CHECK(highest == (255 * 200) >> 8);
}

TEST_CASE("stop ends the tone on a cycle boundary")
{
    ToneSynth synth;
    synth.start(500, 32000, 255);
    for (int i = 0; i < 20; ++i) {
        synth.next();
    }
    synth.stop();
    CHECK(synth.running());
    int samples = 0;
    int last = -1;
    while (synth.running() && samples < 100) {
        last = synth.next();
        ++samples;
    }
    CHECK_FALSE(synth.running());
    CHECK(last == 0);
    CHECK(samples <= 64 - 20 + 1);
    CHECK(synth.next() == 0);
}

TEST_CASE("start while running changes pitch without restarting, 0 Hz is silence")
{
    ToneSynth synth;
    synth.start(0, 32000, 255);
    CHECK_FALSE(synth.running());
    synth.start(800, 32000, 255);
    for (int i = 0; i < 7; ++i) {
        synth.next();
    }
    const int before = synth.next();
    synth.start(900, 32000, 255);
    const int after = synth.next();
    CHECK(after - before < 40);
    CHECK(before - after < 40);
}

TEST_CASE("lib::visit picks the active alternative")
{
    struct A
    {
        int x;
    };
    struct B
    {
    };
    std::variant<A, B> v = A{5};
    CHECK(dmm::lib::visit(dmm::lib::Overload{[](const A &a) { return a.x; }, [](const B &) { return -1; }}, v) == 5);
    v = B{};
    CHECK(dmm::lib::visit(dmm::lib::Overload{[](const A &a) { return a.x; }, [](const B &) { return -1; }}, v) == -1);
}
