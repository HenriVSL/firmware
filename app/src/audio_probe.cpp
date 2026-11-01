// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Henri Lavikainen

#include "audio_probe.h"

#include "aprs.h"

#include "dmm/lib/format.h"
#include "dmm/lib/strings.h"
#include "dmm/os/os.h"
#include "dmm/services/audio_meter.h"
#include "dmm/services/log.h"

#include <cstdint>
#include <optional>
#include <span>

namespace dmm::app::probe {

namespace {

namespace log = services::log;

constexpr std::uint32_t RateHz = 9600;
// The two AFSK tones; 10 ms windows hold whole cycles of both.
constexpr std::uint32_t Tones[] = {1200, 2200};
constexpr std::uint32_t WindowSamples = RateHz / 100;
constexpr std::uint32_t DefaultMs = 1000;
constexpr std::uint32_t MaxMs = 10'000;
constexpr std::uint32_t MaxDumpMs = 5000;
// Samples per dump line, three hex digits (mV) each.
constexpr std::size_t LineSamples = 32;
constexpr os::Ticks PollMs = 10;

struct Request
{
    std::uint32_t ms = DefaultMs;
    bool dump = false;
};

// "audio probe [dump] [ms]"; nothing if the rest does not parse.
std::optional<Request> parse(std::string_view rest)
{
    Request r;
    std::string_view word = lib::takeWord(rest);
    if (word == "dump") {
        r.dump = true;
        word = lib::takeWord(rest);
    }
    if (!word.empty()) {
        const auto ms = lib::parseNumber(word, 10);
        if (!ms || *ms < 10) {
            return std::nullopt;
        }
        r.ms = static_cast<std::uint32_t>(*ms);
    }
    if (!rest.empty() || r.ms > (r.dump ? MaxDumpMs : MaxMs)) {
        return std::nullopt;
    }
    return r;
}

// @p value with one decimal.
template <class Line>
void tenths(Line &line, float value)
{
    const bool negative = value < 0.0F;
    const auto t = static_cast<std::uint32_t>((negative ? -value : value) * 10.0F + 0.5F);
    line << (negative ? "-" : "") << t / 10 << '.' << static_cast<char>('0' + t % 10);
}

class Dumper
{
public:
    explicit Dumper(Platform &platform)
        : m_platform(platform)
    {
    }

    // Adds samples that follow on from sample @p index.
    void add(std::uint32_t index, std::span<const std::uint16_t> mv)
    {
        if (m_count > 0 && index != m_first + m_count) {
            flush();
        }
        for (std::uint16_t v : mv) {
            if (m_count == 0) {
                m_first = index;
            }
            m_line[m_count++] = v;
            ++index;
            if (m_count == LineSamples) {
                flush();
            }
        }
    }

    void flush()
    {
        if (m_count == 0) {
            return;
        }
        {
            auto line = log::info("audio");
            line << "d " << m_first << ' ';
            for (std::size_t i = 0; i < m_count; ++i) {
                line << lib::hex(m_line[i] > 0xFFF ? 0xFFFu : m_line[i], 3);
            }
        }
        m_count = 0;
        if (m_platform.logSink.ready()) {
            log::drain(m_platform.logSink);
        }
    }

private:
    Platform &m_platform;
    std::uint16_t m_line[LineSamples] = {};
    std::size_t m_count = 0;
    std::uint32_t m_first = 0;
};

void run(const Request &request, Platform &platform)
{
    board::AudioSampler &sampler = *platform.audioSampler;
    const std::uint32_t wanted = request.ms * RateHz / 1000;
    // APRS decoding on the MCU pauses meanwhile.
    if (!aprs::lendSampler(true)) {
        aprs::lendSampler(false);
        log::info("audio") << "the sampler is busy";
        return;
    }
    if (!sampler.start(RateHz)) {
        aprs::lendSampler(false);
        log::info("audio") << "cannot sample at " << RateHz << " Hz";
        return;
    }
    log::info("audio") << "probe: " << request.ms << " ms at " << RateHz << " Hz" << (request.dump ? ", samples in mV as hex" : "");
    if (platform.logSink.ready()) {
        log::drain(platform.logSink);
    }

    services::AudioMeter meter(RateHz, Tones, WindowSamples);
    Dumper dumper(platform);
    static std::uint16_t chunk[256];
    std::uint32_t got = 0;
    // Samples read plus samples lost: the index of the next one.
    std::uint32_t timeline = 0;
    std::uint32_t lost = 0;
    const os::Ticks giveUp = os::nowMs() + request.ms + 1000;
    while (got < wanted && static_cast<std::int32_t>(os::nowMs() - giveUp) < 0) {
        os::sleepMs(PollMs);
        for (;;) {
            const std::uint32_t room = wanted - got < sizeof chunk / sizeof chunk[0] ? wanted - got : sizeof chunk / sizeof chunk[0];
            const std::size_t n = sampler.read(std::span(chunk, room));
            timeline += sampler.lostSamples() - lost;
            lost = sampler.lostSamples();
            if (n == 0) {
                break;
            }
            const std::span<const std::uint16_t> samples(chunk, n);
            meter.add(samples);
            if (request.dump) {
                dumper.add(timeline, samples);
            }
            got += static_cast<std::uint32_t>(n);
            timeline += static_cast<std::uint32_t>(n);
        }
    }
    sampler.stop();
    aprs::lendSampler(false);
    dumper.flush();

    const services::AudioMeter::Result r = meter.result();
    auto line = log::info("audio");
    line << "n " << r.samples << " lost " << lost << ", mean " << r.meanMv << " mV, min " << r.minMv << " max " << r.maxMv
         << ", AC ";
    tenths(line, r.acRmsMv);
    line << " mVrms, 1200 Hz ";
    tenths(line, r.toneRmsMv[0]);
    line << ", 2200 Hz ";
    tenths(line, r.toneRmsMv[1]);
    line << " mVrms, ratio ";
    tenths(line, services::AudioMeter::ratioDb(r.toneRmsMv[0], r.toneRmsMv[1]));
    line << " dB";
}

} // namespace

const char *consoleHelp()
{
    return "audio: audio probe [ms] (10-10000, default 1000), audio probe dump [ms] (up to 5000)";
}

bool runCommand(std::string_view command, Platform &platform)
{
    constexpr std::string_view Prefix = "audio probe";
    if (!lib::startsWith(command, Prefix) || (command.size() > Prefix.size() && command[Prefix.size()] != ' ')) {
        return false;
    }
    std::string_view rest(command.data() + Prefix.size(), command.size() - Prefix.size());
    if (!rest.empty()) {
        rest.remove_prefix(1);
    }
    const auto request = parse(rest);
    if (!request) {
        log::info("console") << consoleHelp();
    } else if (platform.audioSampler == nullptr) {
        log::info("console") << "no audio sampler";
    } else {
        run(*request, platform);
    }
    return true;
}

} // namespace dmm::app::probe
