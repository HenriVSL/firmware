// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Henri Lavikainen

#include "dmm/services/audio_meter.h"

#include <doctest/doctest.h>

#include <cmath>
#include <cstdint>

using dmm::services::AudioMeter;

namespace {

constexpr std::uint32_t RateHz = 9600;
constexpr std::uint32_t Tones[] = {1200, 2200};
constexpr std::uint32_t Window = RateHz / 100;
constexpr double Pi = 3.14159265358979323846;

// @p seconds of a1 sin(1200 Hz) + a2 sin(2200 Hz) on @p biasMv.
AudioMeter::Result measure(double a1, double a2, double biasMv, double seconds)
{
    AudioMeter meter(RateHz, Tones, Window);
    const auto n = static_cast<int>(seconds * RateHz);
    for (int i = 0; i < n; ++i) {
        const double t = static_cast<double>(i) / RateHz;
        const double v = biasMv + a1 * std::sin(2 * Pi * 1200 * t) + a2 * std::sin(2 * Pi * 2200 * t + 0.3);
        meter.add(static_cast<std::uint16_t>(std::lround(v)));
    }
    return meter.result();
}

} // namespace

TEST_CASE("audio meter: a 1200 Hz sine")
{
    const auto r = measure(400.0, 0.0, 1100.0, 1.0);
    CHECK(r.samples == RateHz);
    CHECK(r.windows == 100);
    CHECK(r.meanMv == 1100);
    CHECK(r.minMv == 700);
    CHECK(r.maxMv == 1500);
    CHECK(r.acRmsMv == doctest::Approx(400.0 / std::sqrt(2.0)).epsilon(0.005));
    CHECK(r.toneRmsMv[0] == doctest::Approx(400.0 / std::sqrt(2.0)).epsilon(0.005));
    CHECK(r.toneRmsMv[1] < 1.0F);
    CHECK(AudioMeter::ratioDb(r.toneRmsMv[0], r.toneRmsMv[1]) > 40.0F);
}

TEST_CASE("audio meter: tilted tone pair")
{
    // 2200 Hz 6 dB below 1200 Hz.
    const auto r = measure(300.0, 150.0, 1000.0, 0.5);
    CHECK(r.toneRmsMv[0] == doctest::Approx(300.0 / std::sqrt(2.0)).epsilon(0.01));
    CHECK(r.toneRmsMv[1] == doctest::Approx(150.0 / std::sqrt(2.0)).epsilon(0.01));
    CHECK(AudioMeter::ratioDb(r.toneRmsMv[0], r.toneRmsMv[1]) == doctest::Approx(6.02).epsilon(0.01));
    CHECK(r.acRmsMv == doctest::Approx(std::sqrt((300.0 * 300.0 + 150.0 * 150.0) / 2.0)).epsilon(0.01));
}

TEST_CASE("audio meter: silence and limits")
{
    AudioMeter meter(RateHz, Tones, Window);
    CHECK(meter.result().samples == 0);
    for (int i = 0; i < 200; ++i) {
        meter.add(1234);
    }
    const auto r = meter.result();
    CHECK(r.meanMv == 1234);
    CHECK(r.minMv == 1234);
    CHECK(r.maxMv == 1234);
    CHECK(r.acRmsMv == doctest::Approx(0.0));
    CHECK(r.windows == 2);
    CHECK(r.toneRmsMv[0] == doctest::Approx(0.0));
    CHECK(AudioMeter::ratioDb(0.0F, 0.0F) == 0.0F);
    CHECK(AudioMeter::ratioDb(1.0F, 0.0F) == 99.0F);
    CHECK(AudioMeter::ratioDb(0.0F, 1.0F) == -99.0F);
    CHECK(AudioMeter::ratioDb(10.0F, 1.0F) == doctest::Approx(20.0));
}
