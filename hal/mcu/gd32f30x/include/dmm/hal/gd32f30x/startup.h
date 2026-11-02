// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tommi Tauriainen

#pragma once

// Start-up contract. The reset handler sets the stack, enables the FPU,
// points the vector table at this image, masks everything a boot loader
// may have left enabled, initialises RAM, runs static constructors, then
// calls boardMain() with interrupts still disabled; the scheduler enables
// them when it starts.
//
// Interrupt handlers are extern "C" functions named irq<N>Handler, N being
// the interrupt number from irq.h; unhandled ones loop until the watchdog
// restarts the MCU.

#include <cstddef>

namespace dmm::hal::gd32f30x {

/// Defined by the board.
[[noreturn]] void boardMain();

/// The main stack's words never used since start-up. The start-up code
/// and the interrupt handlers run on it.
std::size_t unusedMainStackWords();

} // namespace dmm::hal::gd32f30x
