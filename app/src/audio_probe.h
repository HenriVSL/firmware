// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Henri Lavikainen

#pragma once

#include "dmm/app/app.h"

#include <string_view>

namespace dmm::app::probe {

/// The console's help line for the audio probe.
const char *consoleHelp();

/// Runs "audio probe [dump] [ms]" in the calling task: samples the received
/// audio for that long, logs its levels and its 1200 and 2200 Hz tone
/// levels, and with dump every sample. Waits until done, draining the log
/// itself, so it belongs to the task that drains the log. False if
/// @p command is not an audio probe command.
bool runCommand(std::string_view command, Platform &platform);

} // namespace dmm::app::probe
