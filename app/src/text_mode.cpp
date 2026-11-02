// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Henri Lavikainen

#include "text_mode.h"

#include "gps.h"
#include "radio.h"
#include "ui.h"

#include "dmm/lib/format.h"
#include "dmm/lib/strings.h"
#include "dmm/os/os.h"
#include "dmm/services/log.h"
#include "dmm/services/rtty_demodulator.h"
#include "dmm/services/rtty_loopback.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <optional>

namespace dmm::app::text_mode {

namespace {

namespace log = services::log;
using services::RttyDemodulator;
using services::RttyLoopback;
using services::TextModeSettings;
using services::text_mode::Sending;
using services::text_mode::TextLines;

// The decoder belongs to the aprs task, and so does the loopback while it
// runs.
RttyDemodulator demodulator;
os::Ticks longestPassMs = 0;
bool signalWas = false;
// Received text, kept for the screens; a line of it for the log.
lib::StaticString<64> logLine;
std::atomic<bool> logText{false};
// The decoder's time: cycles spent, samples decoded, the longest pass,
// and the fewest cycles per sample in a pass of at least QuickestOf
// samples, which a task above this one cannot have stretched (in 1/16
// cycle); the same since the loopback started.
std::uint64_t cyclesUsed = 0;
std::uint64_t samplesDecoded = 0;
std::uint32_t longestCycles = 0;
std::uint32_t quickest = 0xFFFFFFFF;
std::uint32_t loopbackQuickest = 0xFFFFFFFF;
constexpr std::size_t QuickestOf = 32;
RttyLoopback loopback;
std::atomic<bool> loopbackRunning{false};
os::Ticks loopbackStarted = 0;
std::uint64_t loopbackCycles0 = 0;
std::uint64_t loopbackSamples0 = 0;
// The loopback's codes decoded again as its characters come back: the
// next code, and the characters heard and wrong.
services::rtty::Ita2Decoder loopbackExpected;
std::size_t loopbackNext = 0;
std::size_t loopbackHeard = 0;
std::size_t loopbackWrong = 0;

// Shared with the other tasks.
os::Mutex lock;
TextLines lines;
services::rtty::CallSpotter spotter;
TextModeSettings settings;
bool settingsChanged = false;
Status shown;
RttyDemodulator::Stats statsShown;
std::atomic<std::uint32_t> changes{0};
std::atomic<std::uint32_t> modeHz{0};
lib::StaticString<services::AprsSettings::CallBytes + 1> ownCall;
lib::StaticString<services::rtty::CallSpotter::MaxCall + 1> theirCall;
std::atomic<bool> dry{false};
std::atomic<std::uint8_t> levelPercent{5};
SendStatus sendShown;
os::Ticks sendChangedAt = 0;
// Sending: one text at a time, encoded here, then copied by the radio or
// read by the loopback until it is done.
os::Mutex sendLock;
std::array<std::uint8_t, radio::MaxTextCodes> codes{};
std::array<char, services::text_mode::MaxSendChars> filled{};
std::atomic<bool> loopbackWanted{false};
std::atomic<bool> loopbackStopWanted{false};
std::atomic<bool> loopbackBusy{false};
std::size_t loopbackLength = 0;
services::RttyModulator::Config loopbackConfig;
float loopbackSnrDb = 99.0F;

// A result shows this long.
constexpr os::Ticks ResultShownMs = 5000;
// The longest one transmission may take.
constexpr std::uint32_t MaxAirMs = 120'000;
constexpr std::uint16_t LeadMs = 300;
constexpr std::uint16_t TailMs = 200;
// The loopback starts with this much noise alone.
constexpr std::uint16_t LoopbackQuietMs = 1000;

RttyDemodulator::Config configFor(const TextModeSettings &s)
{
    RttyDemodulator::Config c;
    c.markHz = services::rtty::markHz(s.tones);
    c.baud = services::rtty::baudRate(s.baud);
    c.reverse = s.reverse;
    c.unshiftOnSpace = s.unshiftOnSpace;
    return c;
}

services::RttyModulator::Config modulatorFor(const TextModeSettings &s)
{
    services::RttyModulator::Config c;
    c.markHz = services::rtty::markHz(s.tones);
    c.baud = services::rtty::baudRate(s.baud);
    c.reverse = s.reverse;
    return c;
}

void flushLog()
{
    if (logLine.size() > 0 && (logText.load() || loopbackRunning)) {
        log::info("text") << "rx \"" << logLine.view() << '"';
    }
    logLine.clear();
}

// The next character the loopback should give back; 0 past the end.
char nextExpected()
{
    while (loopbackNext < loopbackLength) {
        const char c = loopbackExpected.decode(codes[loopbackNext++]);
        if (c != 0) {
            return c;
        }
    }
    return 0;
}

void received(char c)
{
    lock.lock();
    lines.add(c);
    // Our own text in the loopback names no other station.
    if (!loopbackRunning) {
        spotter.add(c);
    }
    lock.unlock();
    changes.store(changes.load() + 1);
    if (loopbackRunning) {
        ++loopbackHeard;
        if (c != nextExpected()) {
            ++loopbackWrong;
        }
    }
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

// "0.62 %" of the CPU for @p cycles over @p samples of audio.
lib::StaticString<16> cpuText(std::uint64_t cycles, std::uint64_t samples)
{
    lib::StaticString<16> s;
    const std::uint64_t available = samples * os::cyclesPerSecond() / RttyDemodulator::SampleRateHz;
    const std::uint64_t hundredths = available == 0 ? 0 : cycles * 10'000 / available;
    s << static_cast<std::uint32_t>(hundredths / 100) << '.'
      << lib::zeroPadded(static_cast<std::uint32_t>(hundredths % 100), 2) << " %";
    return s;
}

// The CPU share of the quickest pass, @p perSample in 1/16 cycle.
lib::StaticString<16> quickestText(std::uint32_t perSample)
{
    if (perSample == 0xFFFFFFFF) {
        return {"-"};
    }
    return cpuText(perSample, 16);
}

std::uint32_t microseconds(std::uint32_t cycles)
{
    return static_cast<std::uint32_t>(static_cast<std::uint64_t>(cycles) * 1'000'000 / os::cyclesPerSecond());
}

// Up to @p n characters of @p s from @p from (substr's range check would
// bring in exception code).
std::string_view part(std::string_view s, std::size_t from, std::size_t n)
{
    from = std::min(from, s.size());
    return {s.data() + from, std::min(n, s.size() - from)};
}

// Decodes @p mv, counting the time it takes.
void decodeTimed(std::span<const std::uint16_t> mv)
{
    const std::uint32_t start = os::cycleCount();
    demodulator.process(mv, received);
    const std::uint32_t used = os::cycleCount() - start;
    cyclesUsed += used;
    samplesDecoded += mv.size();
    longestCycles = std::max(longestCycles, used);
    if (mv.size() >= QuickestOf) {
        const auto perSample = static_cast<std::uint32_t>(static_cast<std::uint64_t>(used) * 16 / mv.size());
        quickest = std::min(quickest, perSample);
        loopbackQuickest = std::min(loopbackQuickest, perSample);
    }
}

// Logs @p text after @p what, a line of it at a time, CR and LF left out.
void logPieces(std::string_view what, std::string_view text)
{
    constexpr std::size_t Piece = 90;
    std::size_t at = 0;
    bool any = false;
    while (at < text.size()) {
        std::size_t end = at;
        while (end < text.size() && end - at < Piece && text[end] != '\r' && text[end] != '\n') {
            ++end;
        }
        if (end > at) {
            log::info("text") << what << " \"" << part(text, at, end - at) << '"';
            any = true;
        }
        at = end;
        while (at < text.size() && (text[at] == '\r' || text[at] == '\n')) {
            ++at;
        }
    }
    if (!any) {
        log::info("text") << what << " nothing";
    }
}

void report()
{
    lock.lock();
    const TextModeSettings s = settings;
    const Status st = shown;
    const RttyDemodulator::Stats stats = statsShown;
    const std::size_t held = lines.count();
    lib::StaticString<services::rtty::CallSpotter::MaxCall + 1> callText;
    callText << (theirCall.size() > 0 ? theirCall.view() : spotter.call());
    lib::StaticString<services::AprsSettings::CallBytes + 1> mine;
    mine << ownCall.view();
    lock.unlock();
    constexpr const char *Powers[] = {"low", "mid", "high", "turbo"};
    log::info("text") << "RTTY " << services::text_mode::frequencyText(s.frequencyHz).view() << " MHz"
                      << (s.frequencyHz == 0 ? " (not set)" : "") << ", " << services::rtty::tonesText(s.tones)
                      << (s.reverse ? " reversed" : "") << ", " << services::rtty::baudText(s.baud) << " Bd, unshift on space "
                      << (s.unshiftOnSpace ? "on" : "off") << ", speaker " << (s.audible ? "on" : "off");
    log::info("text") << "sending: power " << Powers[static_cast<int>(s.power) & 3] << ", level " << levelPercent.load()
                      << " %, callsign " << (mine.size() > 0 ? mine.view() : std::string_view("none"))
                      << (s.addCall ? " added" : " not added") << ", Nordic letters "
                      << (s.nordic == services::rtty::Nordic::Telex ? "AE AA OE" : "A A O") << ", dry run "
                      << (dry.load() ? "on" : "off") << ", "
                      << (sendStatus().state == Sending::Idle ? "idle" : services::text_mode::sendingText(sendStatus().state));
    log::info("text") << "decoder " << (st.decoding ? "running" : "not running") << ", signal "
                      << (st.signal ? "on" : "off") << ", quality " << static_cast<int>(st.quality * 100.0F + 0.5F)
                      << " %, AFC " << static_cast<int>(st.afcHz + (st.afcHz < 0.0F ? -0.5F : 0.5F)) << " Hz";
    log::info("text") << stats.characters << " characters, " << stats.framingErrors << " framing errors, "
                      << stats.unsignalled << " dropped without a signal; " << stats.samples / RttyDemodulator::SampleRateHz
                      << " s of audio, " << stats.signalSamples / RttyDemodulator::SampleRateHz << " s with a signal";
    log::info("text") << "decoding takes " << cpuText(cyclesUsed, samplesDecoded).view() << " of the CPU ("
                      << quickestText(quickest).view() << " in the quickest pass), longest pass "
                      << microseconds(longestCycles) << " us (" << longestPassMs << " ms)";
    log::info("text") << held << " lines held, their call "
                      << (callText.size() > 0 ? callText.view() : std::string_view("none")) << "; log "
                      << (logText.load() ? "on" : "off");
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

// A whole number, perhaps negative.
std::optional<int> parseInt(std::string_view text)
{
    bool negative = false;
    if (!text.empty() && text.front() == '-') {
        negative = true;
        text.remove_prefix(1);
    }
    if (text.empty() || text.size() > 4) {
        return std::nullopt;
    }
    int v = 0;
    for (const char c : text) {
        if (c < '0' || c > '9') {
            return std::nullopt;
        }
        v = v * 10 + (c - '0');
    }
    return negative ? -v : v;
}

// Starts the loopback sendText() asked for; the aprs task only. Kept out
// of decode() to keep its frame small on the aprs task's stack.
[[gnu::noinline]] void takeLoopback()
{
    if (!loopbackWanted.exchange(false)) {
        return;
    }
    sendLock.lock();
    loopbackExpected.reset();
    loopbackNext = 0;
    loopbackHeard = 0;
    loopbackWrong = 0;
    const auto peak = static_cast<std::uint8_t>(std::min<unsigned>(levelPercent.load(), 100) * 255 / 100);
    loopback.start(std::span<const std::uint8_t>(codes.data(), loopbackLength), loopbackConfig, LeadMs, TailMs,
        board::Modem::SampleRateHz, peak, loopbackSnrDb, LoopbackQuietMs, os::nowMs());
    sendLock.unlock();
    loopbackRunning = true;
    loopbackStarted = os::nowMs();
    loopbackCycles0 = cyclesUsed;
    loopbackSamples0 = samplesDecoded;
    loopbackQuickest = 0xFFFFFFFF;
    lib::StaticString<16> snr;
    if (loopbackSnrDb < 99.0F) {
        snr << ", SNR " << static_cast<int>(loopbackSnrDb) << " dB";
    }
    log::info("text") << "loopback: " << loopbackLength << " characters at level " << levelPercent.load() << " %"
                      << snr.view();
}

[[gnu::noinline, gnu::cold]] void endLoopback(bool stopped)
{
    flushLog();
    loopbackRunning = false;
    // Characters not heard at all count as wrong too.
    std::size_t sent = loopbackHeard;
    while (nextExpected() != 0) {
        ++loopbackWrong;
        ++sent;
    }
    const os::Ticks ms = os::nowMs() - loopbackStarted;
    log::info("text") << "loopback " << (stopped ? "stopped" : "done") << ": "
                      << (loopbackWrong == 0 ? std::string_view("exact") : std::string_view("differs")) << ", "
                      << loopbackWrong << " of " << sent << " characters wrong, " << ms / 1000 << '.' << ms / 100 % 10
                      << " s; decoding took "
                      << cpuText(cyclesUsed - loopbackCycles0, samplesDecoded - loopbackSamples0).view() << " of the CPU ("
                      << quickestText(loopbackQuickest).view() << " in the quickest pass), longest pass "
                      << microseconds(longestCycles) << " us";
    loopbackBusy = false;
    sendingChanged(stopped ? Sending::Stopped : Sending::Sent);
}

// Decodes the loopback's audio in place of @p count samples from the
// sampler.
[[gnu::noinline]] void decodeLoopback(std::size_t count)
{
    std::uint16_t block[48];
    while (count > 0 && loopback.running()) {
        const std::size_t n = loopback.read(std::span<std::uint16_t>(block, std::min(count, std::size(block))));
        decodeTimed(std::span<const std::uint16_t>(block, n));
        count -= std::min(count, n);
    }
    if (loopbackStopWanted.exchange(false)) {
        endLoopback(true);
    } else if (!loopback.running()) {
        endLoopback(false);
    }
}

// Kept out of the senders' frames: they run on the ui task's stack too.
[[gnu::noinline, gnu::cold]] void logNotSent(Sending why)
{
    log::info("text") << "not sent: " << services::text_mode::sendingText(why);
}

[[gnu::noinline, gnu::cold]] void logRefused(const char *why)
{
    log::info("text") << "send refused: " << why;
}

// The text to send in filled[0..length), from @p text as the settings say.
Sending prepare(std::string_view text, std::size_t &length)
{
    lock.lock();
    const TextModeSettings s = settings;
    lib::StaticString<services::AprsSettings::CallBytes + 1> mine;
    mine << ownCall.view();
    lib::StaticString<services::rtty::CallSpotter::MaxCall + 1> theirs;
    theirs << (theirCall.size() > 0 ? theirCall.view() : spotter.call());
    lock.unlock();
    lib::StaticString<8> where;
    lib::StaticString<8> utc;
    if (const std::optional<services::GpsFix> fix = gps::latestFix(10 * 60'000)) {
        if (fix->valid) {
            where = services::text_mode::locator(fix->latitudeE7, fix->longitudeE7);
        }
        if (fix->timeValid) {
            utc << lib::zeroPadded(fix->hour, 2) << ':' << lib::zeroPadded(fix->minute, 2);
        }
    }
    using services::text_mode::FillResult;
    const services::text_mode::FillValues values{mine.view(), theirs.view(), where.view(), utc.view()};
    switch (services::text_mode::fill(text, values, filled, length)) {
    case FillResult::Done:
        break;
    case FillResult::NoCall:
        return Sending::NoCall;
    case FillResult::NoTheirCall:
        return Sending::NoTheirCall;
    case FillResult::NoLocator:
        return Sending::NoPosition;
    case FillResult::TooLong:
        return Sending::TooLong;
    }
    while (length > 0 && filled[length - 1] == ' ') {
        --length;
    }
    if (length == 0) {
        return Sending::Nothing;
    }
    if (s.addCall && !services::text_mode::namesCall(std::string_view(filled.data(), length), mine.view())) {
        if (mine.size() == 0) {
            return Sending::NoCall;
        }
        constexpr std::string_view De = " DE ";
        if (length + De.size() + mine.size() > filled.size()) {
            return Sending::TooLong;
        }
        for (const char c : De) {
            filled[length++] = c;
        }
        for (const char c : mine.view()) {
            filled[length++] = c;
        }
    }
    return Sending::Idle;
}

// Sends filled[0..length) (sendLock held).
Sending sendFilled(std::size_t length, bool toLoopback, float snrDb)
{
    lock.lock();
    const TextModeSettings s = settings;
    lock.unlock();
    const std::size_t n = services::rtty::encodeTransmission(std::string_view(filled.data(), length), codes,
        services::rtty::Figures::UsTty, s.nordic);
    const services::RttyModulator::Config config = modulatorFor(s);
    if (n == 0 || services::RttyModulator::airMs(n, config.baud, LeadMs, TailMs) > MaxAirMs) {
        return Sending::TooLong;
    }
    logPieces(toLoopback ? "loopback" : "send", std::string_view(filled.data(), length));
    if (toLoopback) {
        if (!status().decoding) {
            logRefused("the decoder is not running");
            return Sending::Refused;
        }
        loopbackLength = n;
        loopbackConfig = config;
        loopbackSnrDb = snrDb;
        loopbackBusy = true;
        loopbackWanted = true;
        return Sending::OnAir;
    }
    radio::TextTransmission t;
    t.hz = modeHz.load();
    t.config = config;
    t.power = s.power;
    t.leadMs = LeadMs;
    t.tailMs = TailMs;
    t.levelPercent = levelPercent.load();
    if (!radio::sendText(std::span<const std::uint8_t>(codes.data(), n), t)) {
        logRefused("a text is still going out");
        return Sending::Refused;
    }
    return Sending::Waiting;
}

// Sends @p text, filled in, or as it is (@p raw: the self-test's own).
Sending sendText(std::string_view text, bool toLoopback, bool raw, float snrDb)
{
    if (modeHz.load() == 0) {
        logRefused("the text mode is off");
        sendingChanged(Sending::Refused);
        return Sending::Refused;
    }
    // A loopback asked for just as the decoder stopped never starts.
    if (loopbackBusy.load() && !loopbackRunning && !status().decoding) {
        loopbackWanted = false;
        loopbackBusy = false;
    }
    const Sending now = sendStatus().state;
    if (loopbackBusy.load() || now == Sending::Waiting || now == Sending::OnAir) {
        logRefused("a text is still going out");
        return Sending::Refused;
    }
    sendLock.lock();
    std::size_t length = 0;
    Sending result = Sending::Idle;
    if (raw) {
        length = std::min(text.size(), filled.size());
        std::copy_n(text.data(), length, filled.data());
        result = length == 0 ? Sending::Nothing : Sending::Idle;
    } else {
        result = prepare(text, length);
    }
    if (result == Sending::Idle) {
        result = sendFilled(length, toLoopback, snrDb);
    }
    sendLock.unlock();
    if (result != Sending::Waiting && result != Sending::OnAir) {
        logNotSent(result);
    }
    sendingChanged(result);
    return result;
}

// The rest of a console command: a quoted text, or the words as they are.
std::string_view textArgument(std::string_view rest)
{
    if (rest.size() >= 2 && rest.front() == '"' && rest.back() == '"') {
        return part(rest, 1, rest.size() - 2);
    }
    return rest;
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
    takeLoopback();
    const os::Ticks start = os::nowMs();
    if (loopbackRunning) {
        decodeLoopback(mv.size());
    } else {
        decodeTimed(mv);
    }
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
    if (loopbackRunning) {
        endLoopback(true);
    } else if (loopbackWanted.exchange(false)) {
        loopbackBusy = false;
        sendingChanged(Sending::Stopped);
    }
    loopbackStopWanted = false;
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
    const std::string_view c = theirCall.size() > 0 ? theirCall.view() : spotter.call();
    const std::size_t n = std::min(c.size(), out.size());
    std::copy_n(c.data(), n, out.data());
    lock.unlock();
    return {out.data(), n};
}

void modeChanged(std::uint32_t hz)
{
    modeHz.store(hz);
    if (hz == 0) {
        stopSending();
    }
}

void setOwnCall(std::string_view c)
{
    lock.lock();
    ownCall.clear();
    ownCall << part(c, 0, ownCall.capacity());
    lock.unlock();
}

void setTheirCall(std::string_view c)
{
    lock.lock();
    theirCall.clear();
    for (const char ch : part(c, 0, theirCall.capacity())) {
        theirCall << (ch >= 'a' && ch <= 'z' ? static_cast<char>(ch - 'a' + 'A') : ch);
    }
    lock.unlock();
    changes.store(changes.load() + 1);
}

Sending send(std::string_view text, bool toLoopback)
{
    return sendText(text, toLoopback || dry.load(), false, 99.0F);
}

Sending sendMacro(std::size_t index, bool toLoopback)
{
    if (index >= services::text_mode::MacroCount) {
        return Sending::Nothing;
    }
    log::info("text") << "macro " << index + 1 << ' ' << services::text_mode::macro(index).name;
    return send(services::text_mode::macro(index).text, toLoopback);
}

void stopSending()
{
    radio::stopText();
    if (loopbackBusy.load()) {
        loopbackStopWanted = true;
    }
}

void sendingChanged(Sending how)
{
    lock.lock();
    if (how != Sending::OnAir || sendShown.state != Sending::OnAir) {
        sendChangedAt = os::nowMs();
    }
    if (how == Sending::OnAir) {
        sendShown.dry = loopbackBusy.load();
    }
    sendShown.state = how;
    lock.unlock();
    changes.store(changes.load() + 1);
    ui::post(ui::TextSent{how});
}

SendStatus sendStatus()
{
    lock.lock();
    SendStatus s = sendShown;
    const os::Ticks since = os::nowMs() - sendChangedAt;
    lock.unlock();
    if (s.state == Sending::OnAir) {
        s.onAirS = since / 1000;
    } else if (s.state != Sending::Waiting && since >= ResultShownMs) {
        s.state = Sending::Idle;
    }
    return s;
}

bool dryRun()
{
    return dry.load();
}

const char *consoleHelp()
{
    return "text, text mode on|off, text freq <MHz>, text tones high|low, text baud 45|50|75, text reverse on|off, "
           "text usos on|off, text hear on|off, text log on|off, text clear, text send <text>, text loopback <text>, "
           "text macro <1-6> [dry], text macros, text selftest [<SNR dB>] [<text>], text stop, text dry on|off, "
           "text call <call>|clear, text power low|mid|high, text addcall on|off, text nordic plain|telex, "
           "text level <1-20>, text compose";
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
    if ((word == "send" || word == "loopback") && !rest.empty()) {
        send(textArgument(rest), word == "loopback");
        return true;
    }
    if (word == "macro") {
        const std::optional<int> n = parseInt(lib::takeWord(rest));
        if (n && *n >= 1 && *n <= static_cast<int>(services::text_mode::MacroCount) && (rest.empty() || rest == "dry")) {
            sendMacro(static_cast<std::size_t>(*n - 1), rest == "dry");
            return true;
        }
    }
    if (word == "macros" && rest.empty()) {
        for (std::size_t i = 0; i < services::text_mode::MacroCount; ++i) {
            log::info("text") << "macro " << i + 1 << ' ' << services::text_mode::macro(i).name << ": "
                              << services::text_mode::macro(i).text;
        }
        return true;
    }
    if (word == "selftest") {
        // An SNR first if the next word is a number.
        std::string_view after = rest;
        const std::optional<int> snr = parseInt(lib::takeWord(after));
        const std::string_view text = textArgument(snr ? after : rest);
        constexpr std::string_view Default = "RYRYRYRYRYRY THE QUICK BROWN FOX JUMPS OVER THE LAZY DOG 0123456789 ?:-/(),.";
        sendText(text.empty() ? Default : text, true, true, snr ? static_cast<float>(*snr) : 99.0F);
        return true;
    }
    if (word == "stop" && rest.empty()) {
        stopSending();
        return true;
    }
    if (word == "dry" && onOff(rest, on)) {
        dry.store(on);
        log::info("text") << "dry run " << (on ? "on: texts go through the loopback, not on air" : "off");
        return true;
    }
    if (word == "call" && !rest.empty()) {
        setTheirCall(rest == "clear" ? std::string_view() : rest);
        log::info("text") << "their call " << (rest == "clear" ? std::string_view("from the text") : rest);
        return true;
    }
    if (word == "level") {
        const std::optional<int> v = parseInt(rest);
        if (v && *v >= 1 && *v <= 20) {
            levelPercent.store(static_cast<std::uint8_t>(*v));
            log::info("text") << "level " << *v << " %";
            return true;
        }
    }
    if (word == "compose" && rest.empty()) {
        if (!ui::post(ui::TextCompose{})) {
            log::info("console") << "busy";
        }
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
        } else if (word == "power" && (rest == "low" || rest == "mid" || rest == "high")) {
            t = {Field::Power, rest == "low" ? 0u : rest == "mid" ? 1u
                                                                  : 2u};
        } else if (word == "addcall" && onOff(rest, on)) {
            t = {Field::AddCall, on ? 1u : 0u};
        } else if (word == "nordic" && (rest == "plain" || rest == "telex")) {
            t = {Field::Nordic, rest == "telex" ? 1u : 0u};
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
