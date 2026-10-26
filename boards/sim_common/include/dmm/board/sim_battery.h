// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tommi Tauriainen

#pragma once

#include "dmm/services/battery.h"

#include <atomic>

/// A battery whose voltage the simulator sets.
class SimBattery
{
public:
    void setMillivolts(int mv) { m_mv = mv; }
    int readMillivolts() { return m_mv.load(); }
    /// A 2-cell Li-ion pack.
    dmm::services::BatteryThresholds thresholds() const
    {
        return {{6450, 6850, 7450}, 100, 6000, 6100, 5950, 10'000};
    }

private:
    std::atomic<int> m_mv{7800};
};
