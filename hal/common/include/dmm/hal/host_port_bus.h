// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tommi Tauriainen

#pragma once

// 8-bit parallel host port of a peripheral processor (Intel-style: chip
// select, separate read and write strobes), with a status line that says
// when the peripheral has data for the host.

#include <cstddef>
#include <cstdint>

namespace dmm::hal {

class HostPortBus
{
public:
    /// Writes @p count bytes, one write cycle each, in one chip-select.
    virtual void write(const std::uint8_t *bytes, std::size_t count) = 0;
    /// One read cycle.
    virtual std::uint8_t read() = 0;
    /// The peripheral signals that it has data to read.
    virtual bool dataWaiting() = 0;

protected:
    ~HostPortBus() = default;
};

} // namespace dmm::hal
