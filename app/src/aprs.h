// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Henri Lavikainen

#pragma once

#include "dmm/services/codeplug.h"

#include <cstdint>
#include <string_view>
#include <variant>

namespace dmm::app::aprs {

/// Our station, from the codeplug. Its fixed position and power are not
/// used: beacons send the GPS position at the power BeaconPower sets.
struct Station
{
    services::AprsSettings settings;
};
/// Time between automatic beacons; 0 = off.
struct BeaconInterval
{
    std::uint16_t seconds;
};
/// Transmit power for beacons; Low until set.
struct BeaconPower
{
    services::PowerLevel level;
};
/// Sends a beacon as soon as there is a position.
struct BeaconNow
{
};
/// Logs the station and the beacon state.
struct Report
{
};

using Message = std::variant<Station, BeaconInterval, BeaconPower, BeaconNow, Report>;

/// Starts the aprs task. Beacons wait for a Station message.
void start();

/// Queues a message for the aprs task; false when the queue is full.
bool post(const Message &message);

/// The console's APRS commands, for its help.
const char *consoleHelp();

/// Runs an "aprs ..." console command; false if @p command is not one.
bool runCommand(std::string_view command);

} // namespace dmm::app::aprs
