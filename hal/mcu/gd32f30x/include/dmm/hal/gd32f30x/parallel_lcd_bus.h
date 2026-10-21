// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tommi Tauriainen

#pragma once

#include "dmm/hal/gd32f30x/gpio.h"
#include "dmm/hal/lcd_bus.h"

namespace dmm::hal::gd32f30x {

/// 8080-style 8-bit write-only bus driven from GPIO: eight data lines on
/// consecutive pins of one port, plus data/command, chip select, write
/// strobe and reset. The read strobe is not used.
class ParallelLcdBus final : public LcdBus
{
public:
    struct Pins
    {
        Port dataPort;
        std::uint8_t firstDataPin;
        Pin dataCommand; // high = data
        Pin chipSelect;  // low = selected
        Pin write;       // data taken on the rising edge
        Pin reset;       // low = reset
    };

    explicit ParallelLcdBus(const Pins &pins);

    /// Configures the pins, controller deselected and out of reset.
    void configurePins();

    void command(std::uint8_t code) override;
    void data(const std::uint8_t *bytes, std::size_t count) override;
    void repeat(std::uint16_t pixel, std::uint32_t count) override;
    void release() override;
    void setReset(bool active) override;

private:
    void writeByte(std::uint8_t value);

    Pins m_pins;
    std::uintptr_t m_dataSetReset;
    std::uintptr_t m_writeSet;
    std::uintptr_t m_writeClear;
    std::uint32_t m_writeMask;
};

} // namespace dmm::hal::gd32f30x
