// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tommi Tauriainen

#pragma once

#include <cstdint>

namespace dmm::hal {

/// A serial link to a chip with 16-bit registers at 7-bit addresses.
class RegisterBus
{
public:
    virtual std::uint16_t read(std::uint8_t address) = 0;
    virtual void write(std::uint8_t address, std::uint16_t value) = 0;

protected:
    ~RegisterBus() = default;
};

} // namespace dmm::hal
