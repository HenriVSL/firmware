// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Henri Lavikainen

// Decodes RTTY from a recording with the radio's own demodulator: a mono
// WAV file or the log of "audio probe dump". Also writes RTTY audio from
// text, for listening and for checks with other decoders.

#include "../afsk_decode/audio_file.h"

#include "dmm/services/rtty.h"
#include "dmm/services/rtty_demodulator.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace {

using dmm::services::RttyDemodulator;
namespace rtty = dmm::services::rtty;

constexpr std::uint32_t MinRateHz = 8000;

int usage()
{
    std::fprintf(stderr,
        "usage: text-decode [options] <recording>\n"
        "       text-decode [options] --encode <text> <out.wav>\n"
        "  <recording>     a mono WAV file (8000 Hz or more; resampled to %u Hz) or a log\n"
        "                  holding the lines of \"audio probe dump\"\n"
        "  --tones <t>     high (2125/2295, the default), low (1275/1445), or the mark tone in Hz\n"
        "  --shift <hz>    the space tone above mark (170)\n"
        "  --baud <b>      45.45 (the default), 50, 75\n"
        "  --reverse       mark is the higher tone\n"
        "  --no-usos       a space in figures stays in figures\n"
        "  --ita2          the international figures row rather than the US one\n"
        "  --save <wav>    write the audio as the decoder takes it, as a %u Hz WAV\n"
        "  --encode <text> write <text> as RTTY audio at %u Hz to <out.wav>\n",
        RttyDemodulator::SampleRateHz, RttyDemodulator::SampleRateHz, RttyDemodulator::SampleRateHz);
    return 2;
}

// Phase-continuous two-tone audio for @p text, with idle mark around it.
std::vector<float> encode(const std::string &text, const RttyDemodulator::Config &c)
{
    std::vector<float> out;
    const double rate = RttyDemodulator::SampleRateHz;
    double phase = 0.0;
    double due = 0.0;
    auto tone = [&](bool mark, double bits) {
        const bool high = mark == c.reverse;
        const double hz = c.markHz + (high ? c.shiftHz : 0);
        due += bits * rate / c.baud;
        while (due >= 1.0) {
            due -= 1.0;
            phase += 2.0 * 3.14159265358979323846 * hz / rate;
            out.push_back(static_cast<float>(0.5 * std::sin(phase)));
        }
    };
    auto code = [&](std::uint8_t v) {
        tone(false, 1.0);
        for (int b = 0; b < 5; ++b) {
            tone(((v >> b) & 1) != 0, 1.0);
        }
        tone(true, 1.5);
    };
    tone(true, 0.5 * c.baud);
    rtty::Ita2Encoder encoder(c.figures);
    code(rtty::Letters);
    for (const char ch : text) {
        std::uint8_t codes[2];
        const std::size_t n = encoder.encode(ch, codes);
        for (std::size_t i = 0; i < n; ++i) {
            code(codes[i]);
        }
    }
    tone(true, 0.3 * c.baud);
    return out;
}

} // namespace

int main(int argc, char **argv)
{
    RttyDemodulator::Config config;
    std::string save;
    std::string path;
    std::string encodeText;
    bool encoding = false;
    for (int i = 1; i < argc; ++i) {
        const bool more = i + 1 < argc;
        if (std::strcmp(argv[i], "--tones") == 0 && more) {
            const std::string t = argv[++i];
            config.markHz = t == "high" ? 2125
                : t == "low"            ? 1275
                                        : static_cast<std::uint16_t>(std::atoi(t.c_str()));
            if (config.markHz < 300 || config.markHz > 3000) {
                return usage();
            }
        } else if (std::strcmp(argv[i], "--shift") == 0 && more) {
            config.shiftHz = static_cast<std::uint16_t>(std::atoi(argv[++i]));
            if (config.shiftHz < 50 || config.shiftHz > 1000) {
                return usage();
            }
        } else if (std::strcmp(argv[i], "--baud") == 0 && more) {
            config.baud = static_cast<float>(std::atof(argv[++i]));
            if (config.baud < 43.0F || config.baud > 100.0F) {
                return usage();
            }
        } else if (std::strcmp(argv[i], "--reverse") == 0) {
            config.reverse = true;
        } else if (std::strcmp(argv[i], "--no-usos") == 0) {
            config.unshiftOnSpace = false;
        } else if (std::strcmp(argv[i], "--ita2") == 0) {
            config.figures = rtty::Figures::Ita2;
        } else if (std::strcmp(argv[i], "--save") == 0 && more) {
            save = argv[++i];
        } else if (std::strcmp(argv[i], "--encode") == 0 && more) {
            encodeText = argv[++i];
            encoding = true;
        } else if (argv[i][0] == '-' || !path.empty()) {
            return usage();
        } else {
            path = argv[i];
        }
    }
    if (path.empty()) {
        return usage();
    }

    if (encoding) {
        const std::vector<float> samples = encode(encodeText, config);
        if (!afsk_decode::writeWav(path, samples, RttyDemodulator::SampleRateHz)) {
            std::fprintf(stderr, "cannot write %s\n", path.c_str());
            return 1;
        }
        std::printf("%s: %.1f s of RTTY, %u/%u Hz, %.2f Bd\n", path.c_str(),
            static_cast<double>(samples.size()) / RttyDemodulator::SampleRateHz, config.markHz,
            config.markHz + config.shiftHz, static_cast<double>(config.baud));
        return 0;
    }

    afsk_decode::Audio audio;
    std::string error;
    if (!afsk_decode::readAudio(path, audio, error)) {
        std::fprintf(stderr, "%s: %s\n", path.c_str(), error.c_str());
        return 1;
    }
    if (audio.rateHz < MinRateHz) {
        std::fprintf(stderr, "%s: %u Hz is too low; record at %u Hz or more\n", path.c_str(), audio.rateHz, MinRateHz);
        return 1;
    }
    std::printf("%s: %zu samples at %u Hz (%.1f s)", path.c_str(), audio.samples.size(), audio.rateHz,
        static_cast<double>(audio.samples.size()) / audio.rateHz);
    if (audio.rateHz != RttyDemodulator::SampleRateHz) {
        std::printf(", resampled to %u Hz", RttyDemodulator::SampleRateHz);
    }
    if (audio.gapSamples != 0) {
        std::printf(", %u lost samples filled with silence", audio.gapSamples);
    }
    std::printf("\nRTTY %u/%u Hz%s, %.2f Bd\n", config.markHz, config.markHz + config.shiftHz,
        config.reverse ? " reversed" : "", static_cast<double>(config.baud));

    const std::vector<float> samples = afsk_decode::resample(audio, RttyDemodulator::SampleRateHz);
    if (!save.empty() && !afsk_decode::writeWav(save, samples, RttyDemodulator::SampleRateHz)) {
        std::fprintf(stderr, "cannot write %s\n", save.c_str());
        return 1;
    }

    RttyDemodulator demodulator;
    demodulator.configure(config);
    std::string line;
    double lineAt = 0.0;
    bool signal = false;
    float best = 0.0F;
    const auto now = [&demodulator] { return static_cast<double>(demodulator.stats().samples) / RttyDemodulator::SampleRateHz; };
    const auto endLine = [&] {
        if (!line.empty()) {
            std::printf("%8.2f s  %s\n", lineAt, line.c_str());
            line.clear();
        }
    };
    // In blocks, as on the radio; the sample count tells the time.
    constexpr std::size_t Block = 192;
    for (std::size_t at = 0; at < samples.size(); at += Block) {
        const std::size_t n = std::min(Block, samples.size() - at);
        demodulator.process(std::span<const float>(samples.data() + at, n), [&](char c) {
            if (c == '\r' || c == '\n') {
                endLine();
                return;
            }
            if (line.empty()) {
                lineAt = now();
            }
            line.push_back(c >= ' ' && c <= '~' ? c : '.');
        });
        best = std::max(best, demodulator.quality());
        if (demodulator.signal() != signal) {
            signal = demodulator.signal();
            endLine();
            std::printf("%8.2f s  [signal %s, quality %.0f %%, AFC %+.0f Hz]\n", now(), signal ? "on" : "off",
                static_cast<double>(demodulator.quality()) * 100.0, static_cast<double>(demodulator.afcHz()));
        }
    }
    endLine();

    const RttyDemodulator::Stats &s = demodulator.stats();
    std::printf("%u characters, %u framing errors, %u dropped without a signal; signal %.1f s, best quality %.0f %%\n",
        s.characters, s.framingErrors, s.unsignalled, static_cast<double>(s.signalSamples) / RttyDemodulator::SampleRateHz,
        static_cast<double>(best) * 100.0);
    return 0;
}
