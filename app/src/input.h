// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tommi Tauriainen

#pragma once

#include "dmm/app/app.h"
#include "dmm/services/input.h"

namespace dmm::app::input {

/// Starts the input task, which owns @p devices.
void start(board::Input &devices);

} // namespace dmm::app::input
