// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tommi Tauriainen

#pragma once

#include "dmm/services/battery.h"

namespace board {

/// The battery voltage sense on the shared ADC.
class BoardBattery
{
public:
    /// Sets up the sense pin. Call once, before the scheduler starts.
    void configure();

    int readMillivolts();
    /// The radio's 2-cell Li-ion pack.
    dmm::services::BatteryThresholds thresholds() const;
};

} // namespace board
