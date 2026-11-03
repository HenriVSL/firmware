// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Henri Lavikainen

#pragma once

#include "draw.h"

#include "dmm/services/display.h"

#include <cstdint>
#include <string_view>

/// The transmit screen and the activity pages, below the status bar the
/// home screen draws.
namespace dmm::app::ui {

/// What the transmit screen shows.
struct TransmitInfo
{
    /// The channel's or the contact's name, large; on FM the channel's.
    std::string_view target;
    /// Under it: "TG 9" on DMR.
    std::string_view name;
    std::uint32_t seconds = 0;
    /// The DMR slot, 0 on FM.
    std::uint8_t slot = 0;
    std::string_view power;
};

/// The transmit screen in place of the home screen while the user's own
/// call or FM transmission is on the air: whom, for how long, the slot and
/// the power.
void drawTransmit(services::Display &d, const TransmitInfo &t);
/// Only the transmit screen's running time.
void drawTransmitTime(services::Display &d, const TransmitInfo &t);

/// An activity page: a title with a note on its right, then LogRows rows of
/// a time and a line of text, and a hint bar.
inline constexpr int LogRowH = 11;
inline constexpr int LogRows = 8;
void drawLogFrame(services::Display &d, std::string_view title, std::string_view note, std::string_view hint);
/// Row @p row of the page, 0 at the top; empty texts clear it. A selected
/// row stands out.
void drawLogRow(services::Display &d, int row, std::string_view time, std::string_view text, services::Color ink,
    bool selected = false);

} // namespace dmm::app::ui
