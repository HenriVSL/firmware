// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tommi Tauriainen

#pragma once

// The simulators' parts that the app uses as concrete types: the radio's
// calibration format with sample data, and no baseband.

#include "sim_fm.h"

#include "dmm/services/baseband.h"
#include "dmm/services/d878uv2_calibration.h"

namespace dmm::board {

using Fm = SimFm;
using Baseband = services::NoBaseband;
using Calibration = services::D878Uv2Calibration;

static_assert(services::FmTransceiver<Fm>);
static_assert(services::Calibration<Calibration>);

} // namespace dmm::board
