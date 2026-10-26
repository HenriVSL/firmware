// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Henri Lavikainen

#pragma once

#include "dmm/os/os.h"
#include "dmm/services/gps.h"

#include <array>
#include <cstdint>

/// A GPS receiver that sends RMC, GGA and GSV once a second: searching for
/// the first five seconds after start, then a fix at a fixed place.
class SimGps
{
public:
    void start();
    void stop() { m_running = false; }
    std::size_t read(std::span<std::uint8_t> data);
    void write(std::span<const std::uint8_t>) {}
    std::uint32_t lostBytes() const { return 0; }

private:
    void queueSecond(std::uint32_t second);

    bool m_running = false;
    dmm::os::Ticks m_startMs = 0;
    std::uint32_t m_nextSecond = 0;
    std::array<char, 512> m_pending{};
    std::size_t m_pendingLength = 0;
    std::size_t m_pendingRead = 0;
};
