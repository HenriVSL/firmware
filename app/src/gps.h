// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Henri Lavikainen

#pragma once

#include "dmm/board/parts.h"

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

using Message = std::variant<Power, Report, Raw>;

/// Starts the gps task, which owns @p port. The receiver stays off until
/// a Power message.
void start(board::Gps &port);

/// There is a GPS receiver.
bool available();

/// Queues a message for the gps task; false when the queue is full or there
/// is no receiver.
bool post(const Message &message);

} // namespace dmm::app::gps
