// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tommi Tauriainen

#pragma once

// The D878UVII's parts that the app uses as concrete types.

#include "dmm/services/d878uv2_calibration.h"
#include "dmm/services/rda1846_transceiver.h"
#include "dmm/services/sct3258_baseband.h"

namespace dmm::board {

using Fm = services::Rda1846Transceiver;
using Baseband = services::Sct3258Baseband;
using Calibration = services::D878Uv2Calibration;

static_assert(services::FmTransceiver<Fm>);
static_assert(services::Baseband<Baseband>);
static_assert(services::Calibration<Calibration>);

} // namespace dmm::board
