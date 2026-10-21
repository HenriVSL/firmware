// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tommi Tauriainen

#pragma once

// SPI bus to a serial NAND flash: single-bit commands, single- or
// dual-bit data from the chip.

#include <cstddef>
#include <cstdint>

namespace dmm::hal {

class SpiNandBus
{
public:
    virtual void select() = 0;
    virtual void deselect() = 0;
    virtual void write(const std::uint8_t *bytes, std::size_t count) = 0;
    /// One bit per clock on the chip's data-out line.
    virtual void read(std::uint8_t *bytes, std::size_t count) = 0;
    /// Two bits per clock, on both data lines (after a dual-output read
    /// command and its address).
    virtual void readDual(std::uint8_t *bytes, std::size_t count) = 0;

protected:
    ~SpiNandBus() = default;
};

} // namespace dmm::hal
