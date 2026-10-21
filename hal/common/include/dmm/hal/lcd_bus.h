// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tommi Tauriainen

#pragma once

// Command/data bus to a display controller (8080 parallel or 4-wire SPI).

#include <cstddef>
#include <cstdint>

namespace dmm::hal {

class LcdBus
{
public:
    /// Selects the controller and sends one command byte.
    virtual void command(std::uint8_t code) = 0;
    /// Sends parameter or pixel bytes after a command.
    virtual void data(const std::uint8_t *bytes, std::size_t count) = 0;
    /// Sends one 16-bit pixel @p count times, high byte first.
    virtual void repeat(std::uint16_t pixel, std::uint32_t count) = 0;
    /// Deselects the controller, so its pins can be shared.
    virtual void release() = 0;
    /// Drives the controller's reset line; true holds it in reset.
    virtual void setReset(bool active) = 0;

protected:
    ~LcdBus() = default;
};

} // namespace dmm::hal
