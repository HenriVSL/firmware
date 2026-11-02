// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Henri Lavikainen

#include "rtty_signal.h"

#include "dmm/services/rtty.h"
#include "dmm/services/rtty_demodulator.h"
#include "dmm/services/rtty_loopback.h"
#include "dmm/services/rtty_modulator.h"

#include <doctest/doctest.h>

#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

using namespace dmm::services;

namespace {

constexpr std::uint32_t PwmRateHz = 32000;
// 5 % of full scale, as packets go out.
constexpr std::uint8_t LowPeak = 12;

std::vector<std::uint8_t> codesFor(std::string_view text)
{
    std::vector<std::uint8_t> codes(512);
    codes.resize(rtty::encodeTransmission(text, codes));
    return codes;
}

std::vector<std::uint8_t> render(RttyModulator &m)
{
    std::vector<std::uint8_t> out;
    while (m.running()) {
        out.push_back(m.next());
    }
    return out;
}

// The power at @p hz in @p x (Hann window, N samples at @p rate).
double binPower(const std::vector<double> &x, double hz, double rate)
{
    const double w = 2.0 * 3.14159265358979323846 * hz / rate;
    double re = 0.0;
    double im = 0.0;
    const double n = static_cast<double>(x.size());
    for (std::size_t i = 0; i < x.size(); ++i) {
        const double win = 0.5 - 0.5 * std::cos(2.0 * 3.14159265358979323846 * static_cast<double>(i) / n);
        re += win * x[i] * std::cos(w * static_cast<double>(i));
        im -= win * x[i] * std::sin(w * static_cast<double>(i));
    }
    return re * re + im * im;
}

// The noise in 300-3000 Hz around a steady tone at @p toneHz, in dB to the
// tone; 5 Hz bins.
double inBandNoiseDb(const std::vector<double> &x, double toneHz, double rate)
{
    const double tone = binPower(x, toneHz, rate) + binPower(x, toneHz - 5.0, rate) + binPower(x, toneHz + 5.0, rate);
    double noise = 0.0;
    for (double hz = 300.0; hz <= 3000.0; hz += 5.0) {
        if (std::fabs(hz - toneHz) > 15.0) {
            noise += binPower(x, hz, rate);
        }
    }
    return 10.0 * std::log10(noise / tone);
}

std::string decodeLoopback(RttyLoopback &l)
{
    RttyDemodulator d;
    std::string out;
    std::uint16_t block[192];
    for (;;) {
        const std::size_t n = l.read(block);
        if (n == 0) {
            break;
        }
        d.process(std::span<const std::uint16_t>(block, n), [&out](char c) { out.push_back(c); });
    }
    return out;
}

} // namespace

TEST_CASE("rtty transmission: shifts, line ends and the Nordic letters")
{
    const std::vector<std::uint8_t> codes = codesFor("a1\n");
    CHECK(codes == std::vector<std::uint8_t>{rtty::Letters, rtty::CarriageReturn, rtty::LineFeed, 0x03, rtty::FiguresShift, 0x17, rtty::CarriageReturn, rtty::LineFeed, rtty::CarriageReturn, rtty::LineFeed});
    auto back = [](std::string_view text, rtty::Nordic nordic) {
        std::vector<std::uint8_t> sent(64);
        sent.resize(rtty::encodeTransmission(text, sent, rtty::Figures::UsTty, nordic));
        rtty::Ita2Decoder d;
        std::string out;
        for (const std::uint8_t c : sent) {
            if (const char ch = d.decode(c); ch != 0) {
                out.push_back(ch);
            }
        }
        return out;
    };
    CHECK(back("Hyvää yötä, Åke", rtty::Nordic::Plain) == "\r\nHYVAA YOTA, AKE\r\n");
    CHECK(back("Hyvää yötä, Åke", rtty::Nordic::Telex) == "\r\nHYVAEAE YOETAE, AAKE\r\n");
    CHECK(back("x@y*z\r\nq", rtty::Nordic::Plain) == "\r\nXYZ\r\nQ\r\n");
    std::uint8_t small[6];
    CHECK(rtty::encodeTransmission("AB", small) == 0);
    std::uint8_t exact[7];
    CHECK(rtty::encodeTransmission("AB", exact) == 7);
}

TEST_CASE("rtty modulator: a character takes 7.5 bits, after the lead and before the tail")
{
    const std::vector<std::uint8_t> codes(40, 0x0A);
    RttyModulator m;
    m.start(codes, {}, 300, 200, PwmRateHz, 255);
    const std::vector<std::uint8_t> out = render(m);
    const double expected = (0.5 + 40.0 * 7.5 / 45.45) * PwmRateHz;
    CHECK(std::fabs(static_cast<double>(out.size()) - expected) < 2.0 * PwmRateHz / 45.45 / 2.0);
    CHECK(RttyModulator::airMs(40, 45.45F, 300, 200) == 7101);
    CHECK_FALSE(m.running());
    CHECK(m.next() == 0);
}

TEST_CASE("rtty modulator: the tone changes without a jump and stays within its level")
{
    const std::vector<std::uint8_t> codes = codesFor("RYRYRYRY");
    RttyModulator m;
    m.start(codes, {}, 100, 100, PwmRateHz, 255);
    const std::vector<std::uint8_t> out = render(m);
    // The steepest slope of the higher tone at full level, plus a step.
    const double steepest = 127.5 * 2.0 * 3.14159265358979323846 * 2295.0 / PwmRateHz + 1.0;
    int worst = 0;
    for (std::size_t i = 1; i < out.size(); ++i) {
        worst = std::max(worst, std::abs(out[i] - out[i - 1]));
    }
    CHECK(worst <= steepest);
    RttyModulator low;
    low.start(codes, {}, 100, 100, PwmRateHz, LowPeak);
    for (const std::uint8_t v : render(low)) {
        REQUIRE(v <= LowPeak);
    }
}

TEST_CASE("rtty modulator: mark is the lower tone, unless reversed")
{
    for (const bool reverse : {false, true}) {
        RttyModulator m;
        const std::vector<std::uint8_t> none;
        RttyModulator::Config c;
        c.reverse = reverse;
        m.start(none, c, 400, 0, PwmRateHz, 255);
        std::vector<double> x;
        while (m.running() && x.size() < 6400) {
            x.push_back(m.next());
        }
        REQUIRE(x.size() == 6400);
        const double low = binPower(x, 2125.0, PwmRateHz);
        const double high = binPower(x, 2295.0, PwmRateHz);
        CHECK((reverse ? high > 100.0 * low : low > 100.0 * high));
    }
}

TEST_CASE("rtty modulator: carrying the remainder keeps the rounding noise of a low level out of the audio band")
{
    RttyModulator m;
    const std::vector<std::uint8_t> none;
    m.start(none, {}, 1000, 0, PwmRateHz, LowPeak);
    std::vector<double> shaped;
    std::vector<double> rounded;
    double phase = 0.0;
    while (shaped.size() < 6400) {
        shaped.push_back(m.next());
        // The same tone rounded plainly to whole steps.
        phase += 2.0 * 3.14159265358979323846 * 2125.0 / PwmRateHz;
        rounded.push_back(std::round(LowPeak * (1.0 - std::cos(phase)) / 2.0));
    }
    const double shapedDb = inBandNoiseDb(shaped, 2125.0, PwmRateHz);
    const double roundedDb = inBandNoiseDb(rounded, 2125.0, PwmRateHz);
    MESSAGE("noise in 300-3000 Hz: " << shapedDb << " dB carried, " << roundedDb << " dB rounded");
    CHECK(shapedDb < -38.0);
    CHECK(shapedDb < roundedDb - 5.0);
}

TEST_CASE("rtty loopback: the transmit audio at the packet level decodes as sent")
{
    constexpr std::string_view Text = "CQ CQ DE N0CALL N0CALL PSE K\r\nRYRYRY 0123456789 ?:-/(),.";
    const std::vector<std::uint8_t> codes = codesFor(Text);
    RttyLoopback l;
    l.start(codes, {}, 300, 200, PwmRateHz, LowPeak, 99.0F, 500, 1);
    const std::string out = decodeLoopback(l);
    CHECK(out == "\r\n" + std::string(Text) + "\r\n");
    for (const bool reverse : {false, true}) {
        RttyModulator::Config c;
        c.markHz = 1275;
        c.baud = 75.0F;
        c.reverse = reverse;
        RttyLoopback other;
        other.start(codes, c, 300, 200, PwmRateHz, LowPeak, 10.0F, 500, 2);
        RttyDemodulator d;
        d.configure({.markHz = 1275, .baud = 75.0F, .reverse = reverse});
        std::string text;
        std::uint16_t block[192];
        while (const std::size_t n = other.read(block)) {
            d.process(std::span<const std::uint16_t>(block, n), [&text](char ch) { text.push_back(ch); });
        }
        CHECK(text == "\r\n" + std::string(Text) + "\r\n");
    }
}

TEST_CASE("rtty loopback: noise at the chosen ratio")
{
    const std::string text = "THE QUICK BROWN FOX JUMPS OVER THE LAZY DOG 0123456789";
    const std::vector<std::uint8_t> codes = codesFor(text);
    RttyLoopback l;
    l.start(codes, {}, 300, 200, PwmRateHz, LowPeak, 0.0F, 1000, 3);
    const std::string out = decodeLoopback(l);
    CHECK(editDistance(out, "\r\n" + text + "\r\n") <= 2);
    // No signal: noise alone at the chosen spread.
    RttyLoopback quiet;
    quiet.start({}, {}, 0, 0, PwmRateHz, LowPeak, 0.0F, 1000, 4);
    std::vector<std::uint16_t> mv(9600);
    REQUIRE(quiet.read(mv) == 9600);
    double power = 0.0;
    for (const std::uint16_t v : mv) {
        power += (v - 1110.0) * (v - 1110.0);
    }
    // 300 mV peak at 0 dB in 3 kHz: 45000 mV^2 over 3 kHz, 72000 over 4.8.
    CHECK(power / 9600.0 == doctest::Approx(72000.0).epsilon(0.1));
}
