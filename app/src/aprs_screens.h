// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Henri Lavikainen

#pragma once

#include "aprs.h"
#include "standby.h"

#include "dmm/lib/format.h"
#include "dmm/services/display.h"

#include <cstdint>

/// The APRS screens, below the status bar the home screen draws.
namespace dmm::app::ui {

/// "12.3 km" from our fix to the station, or empty without both positions.
lib::StaticString<10> distanceText(const GpsState &gps, const aprs::HeardStation &s);
/// "45 s", "12 min", "3 h": how long ago @p thenMs was.
lib::StaticString<8> ageText(std::uint32_t thenMs, std::uint32_t nowMs);

/// One heard station: its call and age, position, distance and bearing
/// from our fix, altitude, speed and course, its text, and the path it
/// came by.
void drawAprsStation(services::Display &d, const aprs::HeardStation &s, const GpsState &gps, std::uint32_t nowMs);

} // namespace dmm::app::ui
