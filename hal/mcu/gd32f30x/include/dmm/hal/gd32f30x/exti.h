// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tommi Tauriainen

#pragma once

#include "dmm/hal/gd32f30x/gpio.h"

namespace dmm::hal::gd32f30x {

enum class Edge : std::uint8_t
{
    Rising,
    Falling,
    Both,
};

/// Routes @p pin to EXTI line pin.number (one port per line) and raises
/// that line's interrupt on @p edge. The board enables the matching NVIC
/// interrupt and clears the line in its handler.
void enableExti(Pin pin, Edge edge);

/// True if line @p line is pending; clears it.
bool takeExti(unsigned line);

} // namespace dmm::hal::gd32f30x
