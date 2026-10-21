// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tommi Tauriainen

#pragma once

#include <cstdint>

namespace dmm::hal::gd32f30x {

/// The two 12-bit DAC channels: channel 0 on PA4, channel 1 on PA5. The
/// board sets the pin to analog first.
namespace dac {

/// Switches the channel on, unbuffered, with output @p value.
void start(unsigned channel, std::uint16_t value);

/// New output, 0..4095; larger values are clamped.
void write(unsigned channel, std::uint16_t value);

} // namespace dac

} // namespace dmm::hal::gd32f30x
