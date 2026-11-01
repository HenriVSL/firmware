// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Henri Lavikainen

#include "rtty_signal.h"

#include "dmm/services/rtty.h"
#include "dmm/services/rtty_demodulator.h"

#include <doctest/doctest.h>

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

using namespace dmm::services;

namespace {

std::string decodeCodes(rtty::Ita2Decoder &d, std::initializer_list<std::uint8_t> codes)
{
    std::string out;
    for (const std::uint8_t c : codes) {
        const char ch = d.decode(c);
        if (ch != 0) {
            out.push_back(ch);
        }
    }
    return out;
}

std::string decodeAudio(RttyDemodulator &d, const std::vector<std::uint16_t> &mv)
{
    std::string out;
    // In blocks, as on the radio.
    for (std::size_t at = 0; at < mv.size(); at += 192) {
        const std::size_t n = std::min<std::size_t>(192, mv.size() - at);
        d.process(std::span<const std::uint16_t>(mv.data() + at, n), [&out](char c) { out.push_back(c); });
    }
    return out;
}

constexpr std::string_view Sample = "CQ CQ CQ DE N0CALL N0CALL PSE K\r\n"
                                    "THE QUICK BROWN FOX JUMPS OVER THE LAZY DOG 0123456789\r\n"
                                    "RYRYRYRY SEND 73, TEST: (1/2) $5 ? - END\r\n";

struct Condition
{
    const char *name = "clean";
    double markHz = 2125.0;
    double baud = 45.45;
    double toneOffsetHz = 0.0;
    double clockScale = 1.0;
    double tiltDb = 0.0;
    double snrDb = 99.0;
    bool fmNoise = false;
    double clicksPerSecond = 0.0;
    double stopBits = 1.5;
    double peakMv = 300.0;
    bool reverse = false;
};

// The share of characters right after sending Sample under @p c, with
// noise around the transmission.
double decodeRate(const Condition &c, unsigned seed = 1)
{
    RttySignal s;
    s.markHz = c.markHz;
    s.baud = c.baud;
    s.toneOffsetHz = c.toneOffsetHz;
    s.clockScale = c.clockScale;
    s.stopBits = c.stopBits;
    s.reverse = c.reverse;
    s.setTilt(c.tiltDb);
    s.silence(1.0);
    s.text(Sample);
    s.silence(1.0);
    if (c.snrDb < 99.0) {
        s.addNoise(c.snrDb, seed * 11 + 5, c.fmNoise);
    }
    if (c.clicksPerSecond > 0.0) {
        s.addClicks(c.clicksPerSecond, 3.0, seed * 13 + 7);
    }
    RttyDemodulator d;
    RttyDemodulator::Config config;
    config.markHz = static_cast<std::uint16_t>(c.markHz);
    config.baud = static_cast<float>(c.baud);
    config.reverse = c.reverse;
    d.configure(config);
    const std::string out = decodeAudio(d, s.millivolts(c.peakMv));
    const double errors = static_cast<double>(editDistance(out, Sample));
    return 1.0 - std::min(1.0, errors / static_cast<double>(Sample.size()));
}

// Characters passed on from @p seconds of noise alone.
std::size_t charactersFromNoise(double seconds, bool fm, double sigmaMv, unsigned seed)
{
    RttyDemodulator d;
    std::size_t n = 0;
    // In pieces, to keep the buffers small.
    for (double done = 0.0; done < seconds; done += 30.0) {
        RttySignal s;
        s.silence(30.0);
        s.addNoise(0.0, seed++, fm);
        // Scaled so the noise has sigmaMv around the DC level.
        double power = 0.0;
        for (const double v : s.samples()) {
            power += v * v;
        }
        const double scale = sigmaMv / std::sqrt(power / static_cast<double>(s.samples().size()));
        n += decodeAudio(d, s.millivolts(scale)).size();
    }
    return n;
}

} // namespace

TEST_CASE("ita2: letters, figures and the shifts")
{
    rtty::Ita2Decoder d;
    // R and Y, each the other's bits flipped.
    CHECK(decodeCodes(d, {0x0A, 0x15, 0x0A, 0x15}) == "RYRY");
    CHECK(decodeCodes(d, {0x03, 0x04, rtty::FiguresShift, 0x17, 0x13, rtty::Letters, 0x03}) == "A 12A");
    CHECK(decodeCodes(d, {rtty::CarriageReturn, rtty::LineFeed, 0x00}) == "\r\n");
}

TEST_CASE("ita2: a space in figures goes back to letters unless asked not to")
{
    rtty::Ita2Decoder d;
    CHECK(decodeCodes(d, {rtty::FiguresShift, 0x17, rtty::Space, 0x17}) == "1 Q");
    d.reset();
    d.setUnshiftOnSpace(false);
    CHECK(decodeCodes(d, {rtty::FiguresShift, 0x17, rtty::Space, 0x17}) == "1 1");
}

TEST_CASE("ita2: the US and international figures rows differ where they should")
{
    rtty::Ita2Decoder us(rtty::Figures::UsTty);
    rtty::Ita2Decoder ita(rtty::Figures::Ita2);
    CHECK(decodeCodes(us, {rtty::FiguresShift, 0x05, 0x09, 0x0B, 0x11, 0x1E}) == "\a$'\";");
    CHECK(decodeCodes(ita, {rtty::FiguresShift, 0x05, 0x09, 0x0B, 0x11, 0x1E}) == "'\a+=");
}

TEST_CASE("ita2: the encoder adds shifts only where needed, and again after a space")
{
    rtty::Ita2Encoder e;
    std::vector<std::uint8_t> codes;
    for (const char c : std::string_view("a1 2b")) {
        std::uint8_t out[2];
        const std::size_t n = e.encode(c, out);
        codes.insert(codes.end(), out, out + n);
    }
    CHECK(codes == std::vector<std::uint8_t>{0x03, rtty::FiguresShift, 0x17, rtty::Space, rtty::FiguresShift, 0x13, rtty::Letters, 0x19});
    std::uint8_t out[2];
    CHECK(e.encode('@', out) == 0);
    CHECK_FALSE(e.sendable('*'));
    CHECK(e.sendable('z'));
}

TEST_CASE("ita2: every character the encoder takes comes back through the decoder")
{
    for (const rtty::Figures row : {rtty::Figures::UsTty, rtty::Figures::Ita2}) {
        rtty::Ita2Encoder e(row);
        rtty::Ita2Decoder d(row);
        std::string sent;
        std::string back;
        for (int c = 1; c < 128; ++c) {
            const char ch = static_cast<char>(c);
            std::uint8_t out[2];
            const std::size_t n = e.encode(ch, out);
            if (n == 0) {
                continue;
            }
            sent.push_back(ch >= 'a' && ch <= 'z' ? static_cast<char>(ch - 32) : ch);
            for (std::size_t i = 0; i < n; ++i) {
                const char got = d.decode(out[i]);
                if (got != 0) {
                    back.push_back(got);
                }
            }
        }
        CHECK(back == sent);
        CHECK(sent.size() > 50);
    }
}

TEST_CASE("call spotter: the word after DE when it looks like a callsign")
{
    rtty::CallSpotter s;
    for (const char c : std::string_view("CQ CQ DE N0CALL N0CALL K\r\n")) {
        s.add(c);
    }
    CHECK(s.call() == "N0CALL");
    for (const char c : std::string_view("TNX DE ME\r\nN0CALL DE Q0TEST/P K ")) {
        s.add(c);
    }
    CHECK(s.call() == "Q0TEST/P");
    for (const char c : std::string_view("QSL DE Q1AB. ")) {
        s.add(c);
    }
    CHECK(s.call() == "Q1AB");
    CHECK_FALSE(rtty::CallSpotter::looksLikeCall("THE"));
    CHECK_FALSE(rtty::CallSpotter::looksLikeCall("73"));
    CHECK(rtty::CallSpotter::looksLikeCall("Q1AB"));
}

TEST_CASE("rtty demodulator: clean text comes out exactly")
{
    RttySignal s;
    s.silence(0.5);
    s.text(Sample);
    RttyDemodulator d;
    CHECK(decodeAudio(d, s.millivolts(300.0)) == Sample);
    CHECK(d.stats().framingErrors == 0);
}

TEST_CASE("rtty demodulator: decode rate under each condition")
{
    struct Row
    {
        Condition c;
        double atLeast;
    };
    std::vector<Row> rows;
    auto add = [&rows](Condition c, double atLeast) { rows.push_back({c, atLeast}); };
    add({}, 1.0);
    add({.name = "low tones 1275/1445", .markHz = 1275.0}, 1.0);
    add({.name = "50 Bd", .baud = 50.0}, 1.0);
    add({.name = "75 Bd", .baud = 75.0}, 1.0);
    add({.name = "reversed", .reverse = true}, 1.0);
    add({.name = "one stop bit", .stopBits = 1.0}, 1.0);
    add({.name = "tilt +6 dB", .tiltDb = 6.0}, 1.0);
    add({.name = "tilt -6 dB", .tiltDb = -6.0}, 1.0);
    add({.name = "tilt +12 dB", .tiltDb = 12.0}, 0.98);
    add({.name = "tones +15 Hz", .toneOffsetHz = 15.0}, 1.0);
    add({.name = "tones -15 Hz", .toneOffsetHz = -15.0}, 1.0);
    add({.name = "tones +30 Hz", .toneOffsetHz = 30.0}, 0.95);
    add({.name = "speed +2 %", .clockScale = 1.02}, 1.0);
    add({.name = "speed -2 %", .clockScale = 0.98}, 1.0);
    add({.name = "speed +4 %", .clockScale = 1.04}, 0.98);
    add({.name = "white noise SNR 10 dB", .snrDb = 10.0}, 1.0);
    add({.name = "white noise SNR 3 dB", .snrDb = 3.0}, 0.99);
    add({.name = "white noise SNR 0 dB", .snrDb = 0.0}, 0.97);
    add({.name = "white noise SNR -3 dB", .snrDb = -3.0}, 0.85);
    // Below the signal detector's edge: nothing comes out.
    add({.name = "white noise SNR -6 dB", .snrDb = -6.0}, 0.0);
    add({.name = "FM noise SNR 3 dB", .snrDb = 3.0, .fmNoise = true}, 0.99);
    add({.name = "FM noise SNR -3 dB", .snrDb = -3.0, .fmNoise = true}, 0.97);
    add({.name = "FM noise SNR -6 dB", .snrDb = -6.0, .fmNoise = true}, 0.85);
    add({.name = "speed +6 %", .clockScale = 1.06}, 0.98);
    add({.name = "tilt +18 dB", .tiltDb = 18.0}, 0.98);
    add({.name = "clicks 50/s", .clicksPerSecond = 50.0}, 0.95);
    add({.name = "FM noise SNR 0 dB", .snrDb = 0.0, .fmNoise = true}, 0.95);
    add({.name = "clicks 5/s", .clicksPerSecond = 5.0}, 0.97);
    add({.name = "clicks 20/s", .clicksPerSecond = 20.0}, 0.85);
    add({.name = "clipped at 0 mV", .snrDb = 10.0, .peakMv = 1600.0}, 1.0);
    add({.name = "quiet, 30 mV", .snrDb = 10.0, .peakMv = 30.0}, 1.0);
    for (const Row &r : rows) {
        double sum = 0.0;
        constexpr unsigned Seeds = 3;
        for (unsigned seed = 1; seed <= Seeds; ++seed) {
            sum += decodeRate(r.c, seed);
        }
        const double rate = sum / Seeds;
        MESSAGE(std::string(r.c.name) << ": " << rate * 100.0 << " % right");
        CHECK_MESSAGE(rate >= r.atLeast, r.c.name);
    }
}

TEST_CASE("rtty demodulator: a level step in the middle costs little")
{
    RttySignal s;
    s.silence(0.5);
    s.idle(0.5);
    const std::size_t at = s.samples().size() + static_cast<std::size_t>(5.0 * RttySignal::RateHz);
    s.text(Sample, 0.0);
    s.scaleFrom(at, 0.25);
    s.addNoise(20.0, 3);
    RttyDemodulator d;
    const std::string out = decodeAudio(d, s.millivolts(400.0));
    CHECK(editDistance(out, Sample) <= 3);
}

TEST_CASE("rtty demodulator: noise alone passes nothing on")
{
    // Ten minutes each: white noise and an FM receiver's noise with the
    // squelch open, cut at 0 mV, and quiet noise as with it closed.
    CHECK(charactersFromNoise(600.0, false, 600.0, 100) == 0);
    CHECK(charactersFromNoise(600.0, true, 600.0, 200) == 0);
    CHECK(charactersFromNoise(600.0, true, 20.0, 300) == 0);
}

TEST_CASE("rtty demodulator: the signal detector opens on a signal and shuts after it")
{
    RttySignal s;
    s.silence(2.0);
    s.text("RYRYRYRYRYRYRYRYRYRY", 0.5, 0.0);
    const std::size_t end = s.samples().size();
    s.silence(2.0);
    s.addNoise(10.0, 9);
    const std::vector<std::uint16_t> mv = s.millivolts(300.0);
    RttyDemodulator d;
    const auto upTo = [&](std::size_t from, std::size_t to) {
        d.process(std::span<const std::uint16_t>(mv.data() + from, to - from), [](char) {});
    };
    upTo(0, static_cast<std::size_t>(1.9 * RttySignal::RateHz));
    CHECK_FALSE(d.signal());
    CHECK(d.quality() < 0.07F);
    upTo(static_cast<std::size_t>(1.9 * RttySignal::RateHz), end);
    CHECK(d.signal());
    CHECK(d.quality() > 0.3F);
    upTo(end, mv.size());
    CHECK_FALSE(d.signal());
}
