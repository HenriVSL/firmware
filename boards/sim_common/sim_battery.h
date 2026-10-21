// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tommi Tauriainen

#pragma once

#include "dmm/services/battery.h"

#include <atomic>

/// A battery whose voltage the simulator sets.
class SimBattery final : public dmm::services::BatterySensor
{
public:
    void setMillivolts(int mv) { m_mv = mv; }
    int readMillivolts() override { return m_mv.load(); }

private:
    std::atomic<int> m_mv{7800};
};
