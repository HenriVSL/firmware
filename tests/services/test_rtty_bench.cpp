// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Henri Lavikainen

// Measurements of the RTTY demodulator, skipped in the usual run: the
// character error rate against the signal-to-noise ratio, and the time it
// takes per second of audio. Run with
//   tests -ts="rtty bench" --no-skip

#include "rtty_signal.h"

#include "dmm/services/rtty_demodulator.h"

#include <doctest/doctest.h>

#include <chrono>
#include <cstdio>
#include <string>
#include <vector>

using namespace dmm::services;

namespace {

// Made-up text with every letter, figure and the shifts between them: words
// of 2-7 characters, now and then figures, lines of about 60 characters.
std::string benchText(std::size_t length, std::uint32_t seed)
{
    constexpr std::string_view Letters = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    constexpr std::string_view Figures = "0123456789-?:().,/";
    std::string out;
    std::size_t line = 0;
    while (out.size() < length) {
        seed ^= seed << 13;
        seed ^= seed >> 17;
        seed ^= seed << 5;
        const bool figures = seed % 5 == 0;
        const std::size_t n = 2 + (seed >> 8) % 6;
        for (std::size_t i = 0; i < n; ++i) {
            const std::uint32_t r = (seed >> (i * 3)) + static_cast<std::uint32_t>(i) * 7919u;
            out.push_back(figures ? Figures[r % Figures.size()] : Letters[r % Letters.size()]);
        }
        line += n + 1;
        if (line > 60) {
            out += "\r\n";
            line = 0;
        } else {
            out.push_back(' ');
        }
    }
    return out;
}

struct Impairment
{
    double toneOffsetHz = 0.0;
    double clockScale = 1.0;
    bool fm = false;
    double driftHzPerS = 0.0;
};

// The share of characters wrong after sending @p text at @p snrDb.
double errorRate(const std::string &text, double snrDb, const Impairment &im, std::uint32_t seed)
{
    RttySignal s;
    s.toneOffsetHz = im.toneOffsetHz;
    s.clockScale = im.clockScale;
    s.driftHzPerS = im.driftHzPerS;
    s.silence(1.0);
    s.text(text, 1.0, 0.5);
    s.silence(1.0);
    s.addNoise(snrDb, seed, im.fm);
    RttyDemodulator d;
    std::string out;
    const std::vector<std::uint16_t> mv = s.millivolts(300.0);
    for (std::size_t at = 0; at < mv.size(); at += 192) {
        const std::size_t n = std::min<std::size_t>(192, mv.size() - at);
        d.process(std::span<const std::uint16_t>(mv.data() + at, n), [&out](char c) { out.push_back(c); });
    }
    return static_cast<double>(editDistance(out, text)) / static_cast<double>(text.size());
}

} // namespace

TEST_SUITE("rtty bench" * doctest::skip())
{
    TEST_CASE("rtty bench: character error rate against the signal-to-noise ratio")
    {
        const std::string text = benchText(1000, 12345);
        struct Column
        {
            const char *name;
            Impairment im;
        };
        const Column columns[] = {
            {"white", {}},
            {"FM", {.fm = true}},
            {"white +40Hz", {.toneOffsetHz = 40.0}},
            {"white -50Hz", {.toneOffsetHz = -50.0}},
            {"white drift", {.toneOffsetHz = -20.0, .driftHzPerS = 0.2}},
            {"white +3%", {.clockScale = 1.03}},
        };
        std::printf("SNR in 3 kHz | character error rate, %%\n");
        std::printf("%6s", "dB");
        for (const Column &c : columns) {
            std::printf(" %12s", c.name);
        }
        std::printf("\n");
        for (int snr = -9; snr <= 9; ++snr) {
            std::printf("%6d", snr);
            for (const Column &c : columns) {
                double sum = 0.0;
                constexpr int Seeds = 3;
                for (int k = 0; k < Seeds; ++k) {
                    sum += errorRate(text, snr, c.im, static_cast<std::uint32_t>(snr * 100 + k * 7 + 1000));
                }
                std::printf(" %12.2f", 100.0 * sum / Seeds);
            }
            std::printf("\n");
        }
    }

    TEST_CASE("rtty bench: time per second of audio")
    {
        RttySignal s;
        s.silence(1.0);
        s.text(benchText(500, 777));
        s.addNoise(6.0, 3);
        const std::vector<std::uint16_t> mv = s.millivolts(300.0);
        const double seconds = static_cast<double>(mv.size()) / RttySignal::RateHz;
        RttyDemodulator d;
        std::size_t chars = 0;
        constexpr int Rounds = 10;
        const auto start = std::chrono::steady_clock::now();
        for (int r = 0; r < Rounds; ++r) {
            for (std::size_t at = 0; at < mv.size(); at += 192) {
                const std::size_t n = std::min<std::size_t>(192, mv.size() - at);
                d.process(std::span<const std::uint16_t>(mv.data() + at, n), [&chars](char) { ++chars; });
            }
        }
        const double us = std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - start).count();
        std::printf("%.1f us per second of audio on this PC (%.1f ns a sample), %zu characters\n", us / (seconds * Rounds),
            1000.0 * us / (seconds * Rounds * RttySignal::RateHz), chars);
        CHECK(chars > 0);
    }
}
