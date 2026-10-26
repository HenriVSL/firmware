// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Henri Lavikainen

#pragma once

#include "dmm/board/parts.h"
#include "dmm/lib/format.h"

#include <cstdint>
#include <variant>

namespace dmm::app::gps {

/// Switches the receiver and its link on or off.
struct Power
{
    bool on;
};
/// Logs the receiver's state and the latest fix.
struct Report
{
};
/// Logs every sentence the receiver sends (console test).
struct Raw
{
    bool on;
};
/// The satellite systems to use (services::gps_systems bits).
struct Systems
{
    std::uint8_t systems;
};
/// One of the receiver chip's own console commands (console test).
struct ChipCommand
{
    lib::StaticString<64> text;
};

using Message = std::variant<Power, Report, Raw, Systems, ChipCommand>;

/// Starts the gps task, which owns @p port. The receiver stays off until
/// a Power message.
void start(board::Gps &port);

/// The receiver chip's console commands, for the console's help.
const char *debugHelp();

/// Queues a message for the gps task; false when the queue is full or there
/// is no receiver.
bool post(const Message &message);

} // namespace dmm::app::gps
