// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Henri Lavikainen

#include "text_mode.h"

#include "ui.h"

#include "dmm/lib/format.h"
#include "dmm/lib/strings.h"
#include "dmm/os/os.h"
#include "dmm/services/log.h"
#include "dmm/services/rtty_demodulator.h"

#include <algorithm>
#include <atomic>
#include <optional>

namespace dmm::app::text_mode {

namespace {

namespace log = services::log;
using services::RttyDemodulator;
using services::TextModeSettings;
using services::text_mode::TextLines;

// The decoder belongs to the aprs task.
RttyDemodulator demodulator;
os::Ticks longestPassMs = 0;
bool signalWas = false;
// Received text, kept for the screens; a line of it for the log.
lib::StaticString<64> logLine;
std::atomic<bool> logText{false};

// Shared with the other tasks.
os::Mutex lock;
TextLines lines;
services::rtty::CallSpotter spotter;
TextModeSettings settings;
bool settingsChanged = false;
Status shown;
RttyDemodulator::Stats statsShown;
std::atomic<std::uint32_t> changes{0};

RttyDemodulator::Config configFor(const TextModeSettings &s)
{
    RttyDemodulator::Config c;
    c.markHz = services::rtty::markHz(s.tones);
    c.baud = services::rtty::baudRate(s.baud);
    c.reverse = s.reverse;
    c.unshiftOnSpace = s.unshiftOnSpace;
    return c;
}

void flushLog()
{
    if (logLine.size() > 0 && logText.load()) {
        log::info("text") << "rx \"" << logLine.view() << '"';
    }
    logLine.clear();
}

void received(char c)
{
    lock.lock();
    lines.add(c);
    spotter.add(c);
    lock.unlock();
    changes.store(changes.load() + 1);
    if (c == '\r' || c == '\n') {
        flushLog();
    } else if (c >= ' ' && c <= '~') {
        logLine << c;
        if (logLine.size() >= 60) {
            flushLog();
        }
    }
}

// Takes settings configure() left, between samples.
void takeSettings()
{
    lock.lock();
    const bool changed = settingsChanged;
    const TextModeSettings s = settings;
    settingsChanged = false;
    lock.unlock();
    if (changed) {
        demodulator.configure(configFor(s));
    }
}

void publish(bool decoding)
{
    const RttyDemodulator::Stats &st = demodulator.stats();
    lock.lock();
    shown.decoding = decoding;
    shown.signal = decoding && demodulator.signal();
    shown.quality = decoding ? demodulator.quality() : 0.0F;
    shown.afcHz = demodulator.afcHz();
    shown.characters = st.characters;
    shown.errors = st.framingErrors;
    statsShown = st;
    lock.unlock();
}

void report()
{
    lock.lock();
    const TextModeSettings s = settings;
    const Status st = shown;
    const RttyDemodulator::Stats stats = statsShown;
    const std::size_t held = lines.count();
    const std::string_view c = spotter.call();
    lib::StaticString<services::rtty::CallSpotter::MaxCall + 1> callText;
    callText << c;
    lock.unlock();
    log::info("text") << "RTTY " << services::text_mode::frequencyText(s.frequencyHz).view() << " MHz"
                      << (s.frequencyHz == 0 ? " (not set)" : "") << ", " << services::rtty::tonesText(s.tones)
                      << (s.reverse ? " reversed" : "") << ", " << services::rtty::baudText(s.baud) << " Bd, unshift on space "
                      << (s.unshiftOnSpace ? "on" : "off") << ", speaker " << (s.audible ? "on" : "off");
    log::info("text") << "decoder " << (st.decoding ? "running" : "not running") << ", signal "
                      << (st.signal ? "on" : "off") << ", quality " << static_cast<int>(st.quality * 100.0F + 0.5F)
                      << " %, AFC " << static_cast<int>(st.afcHz + (st.afcHz < 0.0F ? -0.5F : 0.5F)) << " Hz";
    log::info("text") << stats.characters << " characters, " << stats.framingErrors << " framing errors, "
                      << stats.unsignalled << " dropped without a signal; " << stats.samples / RttyDemodulator::SampleRateHz
                      << " s of audio, " << stats.signalSamples / RttyDemodulator::SampleRateHz << " s with a signal";
    log::info("text") << held << " lines held, last call " << (callText.size() > 0 ? callText.view() : std::string_view("none"))
                      << "; longest decoding pass " << longestPassMs << " ms; log " << (logText.load() ? "on" : "off");
}

// "144.6125" (MHz) as Hz.
std::optional<std::uint32_t> parseMhz(std::string_view text)
{
    std::uint32_t mhz = 0;
    std::uint32_t fraction = 0;
    std::uint32_t scale = 1'000'000;
    bool point = false;
    bool any = false;
    for (const char c : text) {
        if (c == '.' && !point) {
            point = true;
            continue;
        }
        if (c < '0' || c > '9') {
            return std::nullopt;
        }
        any = true;
        if (!point) {
            mhz = mhz * 10 + static_cast<std::uint32_t>(c - '0');
            if (mhz > 4000) {
                return std::nullopt;
            }
        } else if (scale > 1) {
            scale /= 10;
            fraction += static_cast<std::uint32_t>(c - '0') * scale;
        }
    }
    if (!any) {
        return std::nullopt;
    }
    return mhz * 1'000'000 + fraction;
}

bool onOff(std::string_view word, bool &out)
{
    if (word == "on" || word == "off") {
        out = word == "on";
        return true;
    }
    return false;
}

} // namespace

void configure(const TextModeSettings &s)
{
    lock.lock();
    settings = s;
    settingsChanged = true;
    lock.unlock();
}

void startDecoding()
{
    takeSettings();
    demodulator.reset();
    signalWas = false;
    publish(true);
}

void decode(std::span<const std::uint16_t> mv)
{
    takeSettings();
    const os::Ticks start = os::nowMs();
    demodulator.process(mv, received);
    longestPassMs = std::max(longestPassMs, os::nowMs() - start);
    if (signalWas && !demodulator.signal()) {
        flushLog();
    }
    signalWas = demodulator.signal();
    publish(true);
}

void lostSamples()
{
    demodulator.reset();
}

void stopDecoding()
{
    flushLog();
    publish(false);
}

Status status()
{
    lock.lock();
    const Status s = shown;
    lock.unlock();
    return s;
}

std::uint32_t version()
{
    return changes.load();
}

std::size_t lineCount()
{
    lock.lock();
    const std::size_t n = lines.count();
    lock.unlock();
    return n;
}

std::string_view line(std::size_t back, std::span<char, TextLines::Width> out)
{
    lock.lock();
    const std::string_view l = lines.line(back);
    const std::size_t n = std::min(l.size(), out.size());
    std::copy_n(l.data(), n, out.data());
    lock.unlock();
    return {out.data(), n};
}

std::string_view call(std::span<char, services::rtty::CallSpotter::MaxCall> out)
{
    lock.lock();
    const std::string_view c = spotter.call();
    const std::size_t n = std::min(c.size(), out.size());
    std::copy_n(c.data(), n, out.data());
    lock.unlock();
    return {out.data(), n};
}

const char *consoleHelp()
{
    return "text, text mode on|off, text freq <MHz>, text tones high|low, text baud 45|50|75, text reverse on|off, "
           "text usos on|off, text hear on|off, text log on|off, text clear";
}

bool runCommand(std::string_view command)
{
    if (command == "text") {
        report();
        return true;
    }
    if (!lib::startsWith(command, "text ")) {
        return false;
    }
    std::string_view rest(command.data() + 5, command.size() - 5);
    const std::string_view word = lib::takeWord(rest);
    using Field = ui::TextSetting::Field;
    bool on = false;
    if (word == "log" && onOff(rest, on)) {
        logText.store(on);
        log::info("text") << "log " << (on ? "on" : "off");
        return true;
    }
    if (word == "clear" && rest.empty()) {
        lock.lock();
        lines.clear();
        spotter.reset();
        lock.unlock();
        changes.store(changes.load() + 1);
        return true;
    }
    bool valid = true;
    bool posted = true;
    if (word == "mode" && onOff(rest, on)) {
        posted = ui::post(ui::SetMode{on ? services::DedicatedMode::Text : services::DedicatedMode::Off});
    } else {
        ui::TextSetting t{Field::Frequency, 0};
        if (word == "freq") {
            const std::optional<std::uint32_t> hz = parseMhz(rest);
            valid = hz.has_value();
            t.value = hz.value_or(0);
        } else if (word == "tones" && (rest == "high" || rest == "low")) {
            t = {Field::Tones, rest == "low" ? 1u : 0u};
        } else if (word == "baud" && (rest == "45" || rest == "50" || rest == "75")) {
            t = {Field::Baud, rest == "45" ? 0u : rest == "50" ? 1u
                                                               : 2u};
        } else if (word == "reverse" && onOff(rest, on)) {
            t = {Field::Reverse, on ? 1u : 0u};
        } else if (word == "usos" && onOff(rest, on)) {
            t = {Field::Unshift, on ? 1u : 0u};
        } else if (word == "hear" && onOff(rest, on)) {
            t = {Field::Audible, on ? 1u : 0u};
        } else {
            valid = false;
        }
        if (valid) {
            posted = ui::post(t);
        }
    }
    if (!valid) {
        log::info("console") << consoleHelp();
    } else if (!posted) {
        log::info("console") << "busy";
    }
    return true;
}

} // namespace dmm::app::text_mode
