// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Henri Lavikainen

#pragma once

#include <string_view>

namespace dmm::app::aprs {

/// The console's APRS commands, for its help.
const char *consoleHelp();

/// Runs an "aprs ..." console command; false if @p command is not one.
bool runCommand(std::string_view command);

} // namespace dmm::app::aprs
