// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Henri Lavikainen

#include "aprs_screens.h"

#include "draw.h"

#include "dmm/services/font.h"
#include "dmm/services/nmea.h"

#include <cmath>
#include <iterator>
#include <string_view>

namespace dmm::app::ui {

namespace {

using services::Color;
using services::Display;
using services::Font;
using services::latitudeText;
using services::longitudeText;
namespace fonts = services::fonts;
namespace p = palette;

const Font &Small = fonts::Spleen5x8;
const Font &Body = fonts::Spleen6x12;
const Font &Medium = fonts::Spleen8x16;

} // namespace

namespace {

struct Path
{
    float metres;
    float bearingDeg;
};

// Distance and bearing between two positions, on a flat map around them:
// within a few metres per kilometre at the ranges radio reaches.
Path pathBetween(std::int32_t lat1E7, std::int32_t lon1E7, std::int32_t lat2E7, std::int32_t lon2E7)
{
    constexpr float MetresPerE7 = 6'371'000.0f * 3.14159265f / 180.0f / 1e7f;
    const float meanLat = (static_cast<float>(lat1E7) + static_cast<float>(lat2E7)) / 2.0f * (3.14159265f / 180.0f / 1e7f);
    const float north = static_cast<float>(lat2E7 - lat1E7) * MetresPerE7;
    // Across the date line the short way round.
    std::int64_t dLon = static_cast<std::int64_t>(lon2E7) - lon1E7;
    if (dLon > 1'800'000'000) {
        dLon -= 3'600'000'000;
    } else if (dLon < -1'800'000'000) {
        dLon += 3'600'000'000;
    }
    const float east = static_cast<float>(dLon) * MetresPerE7 * std::cos(meanLat);
    float bearing = std::atan2(east, north) * (180.0f / 3.14159265f);
    if (bearing < 0) {
        bearing += 360;
    }
    return {std::sqrt(north * north + east * east), bearing};
}

const char *compassPoint(float degrees)
{
    constexpr const char *Points[] = {"N", "NE", "E", "SE", "S", "SW", "W", "NW"};
    return Points[static_cast<int>((degrees + 22.5f) / 45.0f) % 8];
}

} // namespace

lib::StaticString<10> distanceText(const GpsState &gps, const aprs::HeardStation &s)
{
    lib::StaticString<10> t;
    if (!gps.fix.valid || !s.hasPosition) {
        return t;
    }
    const Path path = pathBetween(gps.fix.latitudeE7, gps.fix.longitudeE7, s.latitudeE7, s.longitudeE7);
    if (path.metres < 1000) {
        t << static_cast<int>(path.metres) << " m";
    } else if (path.metres < 100'000) {
        const int tenths = static_cast<int>(path.metres / 100);
        t << tenths / 10 << '.' << tenths % 10 << " km";
    } else {
        t << static_cast<int>(path.metres / 1000) << " km";
    }
    return t;
}

lib::StaticString<8> ageText(std::uint32_t thenMs, std::uint32_t nowMs)
{
    lib::StaticString<8> t;
    const std::uint32_t s = (nowMs - thenMs) / 1000;
    if (s < 60) {
        t << s << " s";
    } else if (s < 3600) {
        t << s / 60 << " min";
    } else {
        t << s / 3600 << " h";
    }
    return t;
}

void drawAprsStation(Display &d, const aprs::HeardStation &s, const GpsState &gps, std::uint32_t nowMs)
{
    const int w = d.width();
    const int h = d.height();
    const int top = StandbyScreen::StatusH;
    constexpr const char *Labels[] = {"Lat", "Lon", "Dist", "Alt", "Speed", "Text"};
    const int firstRow = top + Medium.height + 5;
    d.fillRect(0, top, w, h - top, p::Ground);
    drawText(d, Medium, 4, top + 2, s.call, p::Text, p::Ground);
    const lib::StaticString<8> age = ageText(s.heardAtMs, nowMs);
    drawText(d, Body, w - 4 - textWidth(Body, age.view()), top + 4, age.view(), p::Dim, p::Ground);
    for (int i = 0; i < static_cast<int>(std::size(Labels)); ++i) {
        drawText(d, Body, 4, firstRow + i * (Body.height + 1), Labels[i], p::Dim, p::Ground);
    }
    d.fillRect(0, h - StandbyScreen::HintH, w, 1, p::Rule);

    int y = firstRow;
    auto value = [&](std::string_view text) {
        line(d, Body, 46, y, w - 50, text, p::Text);
        y += Body.height + 1;
    };
    value(s.hasPosition ? latitudeText(s.latitudeE7).view() : "-");
    value(s.hasPosition ? longitudeText(s.longitudeE7).view() : "-");
    lib::StaticString<24> v;
    if (gps.fix.valid && s.hasPosition) {
        const Path path = pathBetween(gps.fix.latitudeE7, gps.fix.longitudeE7, s.latitudeE7, s.longitudeE7);
        v << distanceText(gps, s).view() << ' ' << static_cast<int>(path.bearingDeg + 0.5f) % 360 << ' ' << compassPoint(path.bearingDeg);
    } else {
        v << "-";
    }
    value(v.view());
    v.clear();
    if (s.hasAltitude) {
        v << s.altitudeM << " m";
    } else {
        v << "-";
    }
    value(v.view());
    v.clear();
    if (s.hasCourse) {
        // 1 knot = 1.852 km/h.
        v << (s.speedKnots * 1852 + 500) / 1000 << " km/h " << s.courseDeg << " deg";
    } else {
        v << "-";
    }
    value(v.view());
    value(s.text[0] != '\0' ? std::string_view(s.text) : "-");

    v.clear();
    v << (s.via[0] != '\0' ? "via " : "direct") << s.via << ", " << s.packets << (s.packets == 1 ? " packet" : " packets");
    line(d, Small, 3, h - StandbyScreen::HintH + 2, w - 6, v.view(), p::Dim);
}

} // namespace dmm::app::ui
