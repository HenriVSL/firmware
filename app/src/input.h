// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tommi Tauriainen

#pragma once

#include "dmm/services/input.h"

namespace dmm::app::input {

/// Starts the input task, which owns @p devices.
void start(services::InputDevices &devices);

} // namespace dmm::app::input
