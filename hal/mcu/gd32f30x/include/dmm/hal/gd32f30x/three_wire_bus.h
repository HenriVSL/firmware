// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tommi Tauriainen

#pragma once

#include "dmm/hal/gd32f30x/gpio.h"
#include "dmm/hal/register_bus.h"

namespace dmm::hal::gd32f30x {

/// Three-wire serial on GPIO pins: an enable line (low for a frame), a
/// clock idling low and one data line used both ways. A frame is an
/// address byte (top bit set for a read) and 16 data bits, most
/// significant bit first; the chip takes each bit on the rising edge.
class ThreeWireBus final : public RegisterBus
{
public:
    struct Pins
    {
        Pin enable;
        Pin clock;
        Pin data;
    };

    explicit ThreeWireBus(const Pins &pins);

    void configurePins();

    std::uint16_t read(std::uint8_t address) override;
    void write(std::uint8_t address, std::uint16_t value) override;

private:
    void sendByte(std::uint8_t byte);

    Pins m_pins;
};

} // namespace dmm::hal::gd32f30x
