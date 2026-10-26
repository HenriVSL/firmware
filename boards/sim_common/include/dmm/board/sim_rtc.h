// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Henri Lavikainen

#pragma once

#include "dmm/services/clock.h"

#include <chrono>
#include <cstdint>

/// The PC's clock, plus whatever the app sets it ahead or back by.
class SimRtc
{
public:
    bool start() { return true; }
    std::uint32_t seconds() const { return static_cast<std::uint32_t>(pcSeconds() + m_offset); }
    void setSeconds(std::uint32_t utc) { m_offset = static_cast<std::int64_t>(utc) - pcSeconds(); }

private:
    static std::int64_t pcSeconds()
    {
        return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    }

    std::int64_t m_offset = 0;
};
