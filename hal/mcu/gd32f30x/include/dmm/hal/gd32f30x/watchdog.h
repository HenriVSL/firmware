// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tommi Tauriainen

#pragma once

#include <cstdint>

namespace dmm::hal::gd32f30x {

/// Starts the free watchdog, which runs from its own ~40 kHz RC oscillator
/// and restarts the MCU unless fed within about @p timeoutMs (at most
/// 3200 ms; the RC tolerance makes the real time 0.7 to 1.3 times that).
/// Once started it cannot be stopped.
void startWatchdog(std::uint32_t timeoutMs);

void feedWatchdog();

} // namespace dmm::hal::gd32f30x
