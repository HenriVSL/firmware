// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tommi Tauriainen

#pragma once

#include "dmm/drivers/radio/rda1846.h"
#include "dmm/services/storage.h"

namespace board {

/// The FM transceiver's register values on this radio, its factory
/// tuning. The radio carries either the S or the N version of the chip;
/// the radio information block in the data flash names it. Without
/// readable storage the S version is assumed.
const dmm::drivers::radio::Rda1846::Tuning &fmTuning(dmm::services::Storage *storage);

} // namespace board
