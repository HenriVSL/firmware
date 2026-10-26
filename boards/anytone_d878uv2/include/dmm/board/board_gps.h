// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Henri Lavikainen

#pragma once

#include "dmm/services/gps.h"

#include <cstddef>
#include <cstdint>
#include <span>

namespace board {

/// The GPS receiver's serial link. Its supply runs through the FM
/// transceiver, which the radio task switches.
class BoardGps
{
public:
    /// Pins pulled low, link closed. Call once, before the scheduler starts.
    void configure();

    void start();
    void stop();
    std::size_t read(std::span<std::uint8_t> data);
    void write(std::span<const std::uint8_t> data);
    std::uint32_t lostBytes() const;

    /// Call from the link's interrupt.
    static void onInterrupt();
};

} // namespace board
