// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tommi Tauriainen

#pragma once

#include "dmm/app/app.h"
#include "dmm/services/input.h"

#include <cstdint>

namespace dmm::app::input {

/// Starts the input task, which owns @p devices.
void start(board::Input &devices);

/// Turns the channel knob by @p clicks, negative for back, as if by hand
/// (console, for tests). Any task.
void turnKnob(std::int32_t clicks);

} // namespace dmm::app::input
