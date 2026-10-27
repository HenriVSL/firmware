// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Henri Lavikainen

#include "aprs.h"

#include "gps.h"
#include "radio.h"

#include "dmm/lib/format.h"
#include "dmm/lib/strings.h"
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

// The destination names the software (an experimental APZ tocall).
constexpr std::string_view Destination = "APZDMM";
// A position older than this is not sent.
constexpr std::uint32_t MaxFixAgeMs = 5 * 60'000;

// Our station; no callsign until one is set.
std::optional<ax25::Address> callsign;
char symbolTable = '/';
char symbol = '[';
lib::StaticString<43> comment{"DeltaMM"};
std::uint16_t leadMs = 300;
std::uint8_t levelPercent = 5;

const char *const Path[] = {"WIDE1-1", "WIDE2-1"};

void logSettings()
{
    log::info("aprs") << "call " << (callsign ? ax25::addressText(*callsign).view() : std::string_view("not set")) << ", symbol "
                      << symbolTable << symbol << ", comment \"" << comment.view() << "\", lead " << leadMs << " ms, level "
                      << levelPercent << " %";
}

services::aprs::Position ourPosition(const services::GpsFix &f)
{
    services::aprs::Position p;
    p.latitudeE7 = f.latitudeE7;
    p.longitudeE7 = f.longitudeE7;
    p.symbolTable = symbolTable;
    p.symbol = symbol;
    // 1 knot = 1.852 km/h; course and speed only while moving.
    const std::uint32_t knots = (f.speedDeciKmh * 10u + 926) / 1852;
    if (knots >= 2) {
        p.hasCourse = true;
        const std::uint16_t degrees = static_cast<std::uint16_t>((f.courseDeciDeg + 5) / 10 % 360);
        p.courseDeg = degrees == 0 ? 360 : degrees;
        p.speedKnots = static_cast<std::uint16_t>(knots);
    }
    if (f.quality != 0) {
        p.hasAltitude = true;
        p.altitudeM = (f.altitudeDm + (f.altitudeDm < 0 ? -5 : 5)) / 10;
    }
    p.comment << comment.view();
    return p;
}

// Its frame is too big for the console task's stack, so it is kept here.
ax25::Frame frame;
std::array<std::uint8_t, ax25::MaxFrameBytes> frameBytes{};

[[gnu::noinline]] void sendPosition()
{
    if (!callsign) {
        log::warning("aprs") << "no callsign: aprs call <CALL-SSID>";
        return;
    }
    const std::optional<services::GpsFix> fix = gps::latestFix(MaxFixAgeMs);
    if (!fix) {
        log::warning("aprs") << "no GPS fix in the last " << MaxFixAgeMs / 60'000 << " min";
        return;
    }
    frame.path.clear();
    frame.source = *callsign;
    frame.destination = *ax25::parseAddress(Destination);
    for (const char *hop : Path) {
        frame.path.pushBack(*ax25::parseAddress(hop));
    }
    frame.info = services::aprs::positionReport(ourPosition(*fix), false);
    const std::size_t n = ax25::encode(frame, frameBytes);
    if (!radio::sendPacket({frameBytes.data(), n})) {
        log::warning("aprs") << "not queued: a packet is already waiting, or there is no modem";
        return;
    }
    auto line = log::info("aprs");
    line << "queued ";
    ax25::writeText(line, frame);
}

} // namespace

const char *consoleHelp()
{
    return "aprs, aprs call <CALL-SSID>, aprs symbol <table><code>, aprs comment <text>, aprs lead <ms>, aprs level <%>, "
           "aprs send";
}

bool runCommand(std::string_view command)
{
    if (command == "aprs") {
        logSettings();
        return true;
    }
    if (!lib::startsWith(command, "aprs ")) {
        return false;
    }
    std::string_view rest(command.data() + 5, command.size() - 5);
    const std::string_view word = lib::takeWord(rest);
    if (word == "send" && rest.empty()) {
        sendPosition();
    } else if (word == "call") {
        callsign = ax25::parseAddress(rest);
        if (!callsign) {
            log::warning("aprs") << "not a callsign: " << rest;
        }
        logSettings();
    } else if (word == "symbol" && rest.size() == 2) {
        symbolTable = rest[0];
        symbol = rest[1];
        logSettings();
    } else if (word == "comment") {
        comment.clear();
        comment << rest;
        logSettings();
    } else if ((word == "lead" || word == "level") && !rest.empty()) {
        const std::optional<std::int32_t> n = lib::parseNumber(rest, 10);
        if (!n || (word == "lead" && (*n < 10 || *n > 2000)) || (word == "level" && (*n < 1 || *n > 100))) {
            log::warning("aprs") << "out of range: " << command;
            return true;
        }
        if (word == "lead") {
            leadMs = static_cast<std::uint16_t>(*n);
        } else {
            levelPercent = static_cast<std::uint8_t>(*n);
        }
        radio::post(radio::PacketSettings{leadMs, levelPercent});
        logSettings();
    } else {
        return false;
    }
    return true;
}

} // namespace dmm::app::aprs
