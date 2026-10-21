// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tommi Tauriainen

#pragma once

#include "dmm/hal/gd32f30x/gpio.h"
#include "dmm/hal/host_port_bus.h"

namespace dmm::hal::gd32f30x {

/// An Intel-style 8-bit host port on GPIO: eight data lines on consecutive
/// pins of one port, active-low chip select, read and write strobes, and a
/// status input that is low while the peripheral has data.
class ParallelHostPort final : public HostPortBus
{
public:
    struct Pins
    {
        Port dataPort;
        std::uint8_t firstDataPin;
        Pin chipSelect;
        Pin write;
        Pin read;
        Pin status;
    };

    explicit ParallelHostPort(const Pins &pins);

    /// Strobes and select idle (high), data lines as inputs.
    void configurePins();
    /// Data and status lines as plain inputs, as after reset, so nothing
    /// feeds a chip that is powered off. The strobes are left to the caller.
    void release();

    void write(const std::uint8_t *bytes, std::size_t count) override;
    std::uint8_t read() override;
    bool dataWaiting() override;

private:
    void driveData(bool drive);

    Pins m_pins;
    bool m_driving = false;
};

} // namespace dmm::hal::gd32f30x
