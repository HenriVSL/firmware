// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Henri Lavikainen

#include "aprs.h"

#include "gps.h"
#include "radio.h"
#include "ui.h"

#include "dmm/lib/format.h"
#include "dmm/lib/strings.h"
#include "dmm/lib/variant.h"
#include "dmm/os/os.h"
#include "dmm/services/aprs.h"
#include "dmm/services/ax25.h"
#include "dmm/services/log.h"

#include <array>
#include <cstdint>
#include <optional>

namespace dmm::app::aprs {

namespace {

namespace log = services::log;
namespace ax25 = services::ax25;
using services::AprsSettings;
using services::GpsSchedule;

os::Task<512> aprsTask;
os::Queue<Message, 4> inbox;

// The destination names the software (an experimental APZ tocall).
constexpr std::string_view Destination = "APZDMM";
// A beacon sends a position at most this old, and waits this long for one.
constexpr std::uint32_t MaxFixAgeMs = 2 * 60'000;
constexpr os::Ticks WaitForFixMs = 3 * 60'000;
// How often a waiting beacon looks again.
constexpr os::Ticks RetryMs = 5000;
constexpr os::Ticks IdleMs = 1000;
// Fewer satellites than this give a poor altitude, which is left out.
constexpr std::uint8_t AltitudeSatellites = 5;

std::optional<AprsSettings> station;
std::uint16_t intervalS = 0;
services::PowerLevel power = services::PowerLevel::Low;
os::Ticks nextBeaconAt = 0;
// A beacon waiting for a position or for the radio, since when.
bool pending = false;
os::Ticks pendingSince = 0;
os::Ticks nextTry = 0;
bool waitingForFix = false;

// Too big for the stack.
ax25::Frame frame;
std::array<std::uint8_t, ax25::MaxFrameBytes> frameBytes{};

lib::StaticString<12> frequencyText(std::uint32_t hz)
{
    lib::StaticString<12> s;
    s << hz / 1'000'000 << '.' << lib::zeroPadded(hz / 1000 % 1000, 3);
    return s;
}

void setGpsNeed(std::uint32_t intervalMs)
{
    gps::post(gps::Need{GpsSchedule::Client::Aprs, intervalMs});
}

void report()
{
    if (!station) {
        log::info("aprs") << "no station: the codeplug has no APRS callsign";
        return;
    }
    const AprsSettings &s = *station;
    constexpr const char *Powers[] = {"low", "mid", "high", "turbo"};
    log::info("aprs") << s.callsign << '-' << s.ssid << ", path " << (s.path[0] != '\0' ? s.path : "none") << ", symbol "
                      << s.symbolTable << s.symbol << ", " << frequencyText(s.frequencyHz).view() << " MHz "
                      << (s.wide ? "wide" : "narrow") << ", power " << Powers[static_cast<int>(power) & 3];
    log::info("aprs") << "comment \"" << s.comment << "\"";
    auto line = log::info("aprs");
    if (intervalS == 0) {
        line << "beacons off";
    } else {
        line << "beacon every " << intervalS << " s, next in "
             << static_cast<std::int32_t>(nextBeaconAt - os::nowMs()) / 1000 << " s";
    }
    if (pending) {
        line << "; one waiting " << (waitingForFix ? "for a position" : "for the radio");
    }
}

services::aprs::Position ourPosition(const services::GpsFix &f)
{
    services::aprs::Position p;
    p.latitudeE7 = f.latitudeE7;
    p.longitudeE7 = f.longitudeE7;
    p.symbolTable = station->symbolTable;
    p.symbol = station->symbol;
    // 1 knot = 1.852 km/h; course and speed only while moving.
    const std::uint32_t knots = (f.speedDeciKmh * 10u + 926) / 1852;
    if (knots >= 2) {
        p.hasCourse = true;
        const auto degrees = static_cast<std::uint16_t>((f.courseDeciDeg + 5) / 10 % 360);
        p.courseDeg = degrees == 0 ? 360 : degrees;
        p.speedKnots = static_cast<std::uint16_t>(knots);
    }
    if (f.satellitesUsed >= AltitudeSatellites) {
        p.hasAltitude = true;
        p.altitudeM = (f.altitudeDm + (f.altitudeDm < 0 ? -5 : 5)) / 10;
    }
    p.comment << station->comment;
    return p;
}

// The frame for a position report from @p fix; false if the station's
// callsign or path does not make one.
bool buildFrame(const services::GpsFix &fix)
{
    lib::StaticString<10> call;
    call << station->callsign << '-' << station->ssid;
    const auto source = ax25::parseAddress(call.view());
    if (!source) {
        return false;
    }
    frame.source = *source;
    frame.destination = *ax25::parseAddress(Destination);
    frame.path.clear();
    std::string_view path(station->path);
    while (!path.empty()) {
        const std::size_t comma = path.find(',');
        const std::string_view hop(path.data(), comma == std::string_view::npos ? path.size() : comma);
        const auto a = ax25::parseAddress(hop);
        if (!a || !frame.path.pushBack(*a)) {
            return false;
        }
        path.remove_prefix(comma == std::string_view::npos ? path.size() : comma + 1);
    }
    frame.info = services::aprs::positionReport(ourPosition(fix), false);
    return true;
}

void endPending()
{
    pending = false;
    waitingForFix = false;
    setGpsNeed(static_cast<std::uint32_t>(intervalS) * 1000);
}

// Tries to queue the waiting beacon.
void tryBeacon()
{
    const os::Ticks now = os::nowMs();
    if (!station) {
        log::warning("aprs") << "beacon not sent: the codeplug has no APRS callsign";
        ui::post(ui::AprsProblem{ui::AprsProblem::Kind::NoCallsign});
        pending = false;
        return;
    }
    const std::optional<services::GpsFix> fix = gps::latestFix(MaxFixAgeMs);
    if (!fix) {
        if (now - pendingSince >= WaitForFixMs) {
            log::warning("aprs") << "beacon not sent: no GPS position within " << WaitForFixMs / 60'000 << " min";
            ui::post(ui::AprsProblem{ui::AprsProblem::Kind::NoFix});
            endPending();
            return;
        }
        if (!waitingForFix) {
            // Positions every second until one comes.
            waitingForFix = true;
            setGpsNeed(1000);
            log::info("aprs") << "beacon waits for a GPS position";
        }
        nextTry = now + RetryMs;
        return;
    }
    if (!buildFrame(*fix)) {
        log::warning("aprs") << "beacon not sent: the callsign or path is not valid";
        ui::post(ui::AprsProblem{ui::AprsProblem::Kind::NoCallsign});
        endPending();
        return;
    }
    const std::size_t n = ax25::encode(frame, frameBytes);
    const radio::PacketChannel channel{station->frequencyHz, station->wide, power};
    if (n == 0 || !radio::sendPacket({frameBytes.data(), n}, channel)) {
        // The radio has a packet already; look again soon.
        if (now - pendingSince >= WaitForFixMs) {
            ui::post(ui::AprsProblem{ui::AprsProblem::Kind::Busy});
            endPending();
        } else {
            nextTry = now + RetryMs;
        }
        return;
    }
    auto line = log::info("aprs");
    line << "beacon ";
    ax25::writeText(line, frame);
    endPending();
}

void startBeacon()
{
    if (pending) {
        return;
    }
    pending = true;
    pendingSince = os::nowMs();
    nextTry = pendingSince;
}

void handle(const Message &message)
{
    lib::visit(lib::Overload{
                   [](const Station &s) {
                       if (s.settings.callsign[0] == '\0') {
                           return;
                       }
                       station = s.settings;
                       intervalS = s.settings.beaconIntervalS;
                       nextBeaconAt = os::nowMs() + static_cast<os::Ticks>(intervalS) * 1000;
                       setGpsNeed(static_cast<std::uint32_t>(intervalS) * 1000);
                       report();
                   },
                   [](const BeaconInterval &b) {
                       intervalS = b.seconds;
                       // The first beacon goes as soon as there is a position.
                       nextBeaconAt = os::nowMs();
                       if (!pending) {
                           setGpsNeed(static_cast<std::uint32_t>(intervalS) * 1000);
                       }
                       log::info("aprs") << (intervalS == 0 ? "beacons off" : "beacons on");
                   },
                   [](const BeaconPower &p) { power = p.level; },
                   [](const BeaconNow &) { startBeacon(); },
                   [](const Report &) { report(); },
               },
        message);
}

void aprsMain(void *)
{
    while (!os::stopRequested()) {
        Message m;
        if (inbox.receive(m, pending ? RetryMs : IdleMs)) {
            handle(m);
        }
        const os::Ticks now = os::nowMs();
        if (intervalS != 0 && static_cast<std::int32_t>(now - nextBeaconAt) >= 0) {
            nextBeaconAt = now + static_cast<os::Ticks>(intervalS) * 1000;
            startBeacon();
        }
        if (pending && static_cast<std::int32_t>(now - nextTry) >= 0) {
            tryBeacon();
        }
    }
}

// Packet settings the console last set.
std::uint16_t leadMs = 300;
std::uint8_t levelPercent = 5;

} // namespace

void start()
{
    aprsTask.start("aprs", 2, aprsMain, nullptr);
}

bool post(const Message &message)
{
    return inbox.send(message);
}

const char *consoleHelp()
{
    return "aprs, aprs send, aprs every <s> (0 = off), aprs lead <ms>, aprs level <%>";
}

bool runCommand(std::string_view command)
{
    if (command == "aprs") {
        post(Report{});
        return true;
    }
    if (!lib::startsWith(command, "aprs ")) {
        return false;
    }
    std::string_view rest(command.data() + 5, command.size() - 5);
    const std::string_view word = lib::takeWord(rest);
    if (word == "send" && rest.empty()) {
        post(BeaconNow{});
        return true;
    }
    const std::optional<std::int32_t> n = lib::parseNumber(rest, 10);
    if (word == "every" && n && *n <= 65535) {
        post(BeaconInterval{static_cast<std::uint16_t>(*n)});
    } else if (word == "lead" && n && *n >= 10 && *n <= 2000) {
        leadMs = static_cast<std::uint16_t>(*n);
        radio::post(radio::PacketSettings{leadMs, levelPercent});
    } else if (word == "level" && n && *n >= 1 && *n <= 100) {
        levelPercent = static_cast<std::uint8_t>(*n);
        radio::post(radio::PacketSettings{leadMs, levelPercent});
    } else {
        return false;
    }
    return true;
}

} // namespace dmm::app::aprs
