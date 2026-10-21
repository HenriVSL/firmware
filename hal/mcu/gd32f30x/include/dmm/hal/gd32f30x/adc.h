// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tommi Tauriainen

#pragma once

#include <cstdint>

namespace dmm::hal::gd32f30x {

/// ADC0 for occasional single readings (knobs, battery). The ADC clock is
/// APB2 / 8 = 15 MHz, each reading about 17 us. The board sets the input
/// pins to analog.
class Adc0
{
public:
    /// Powers up and calibrates. Call once.
    void start();

    /// One 12-bit reading of @p channel (0..17), 0..4095.
    std::uint16_t read(unsigned channel);
};

} // namespace dmm::hal::gd32f30x
