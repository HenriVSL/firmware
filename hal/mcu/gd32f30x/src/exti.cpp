// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tommi Tauriainen

#include "dmm/hal/gd32f30x/exti.h"

#include "dmm/hal/gd32f30x/registers.h"

namespace dmm::hal::gd32f30x {

void enableExti(Pin pin, Edge edge)
{
    reg(rcu::Apb2En) |= rcu::AfioClock;
    [[maybe_unused]] const std::uint32_t enabled = reg(rcu::Apb2En);

    const unsigned line = pin.number;
    const unsigned shift = 4 * (line % 4);
    volatile std::uint32_t &source = reg(afio::extiSource(line));
    source = (source & ~(0xFu << shift)) | (static_cast<std::uint32_t>(pin.port) << shift);

    const std::uint32_t bit = 1u << line;
    if (edge != Edge::Falling) {
        reg(exti::Rten) |= bit;
    }
    if (edge != Edge::Rising) {
        reg(exti::Ften) |= bit;
    }
    reg(exti::Pd) = bit;
    reg(exti::Inten) |= bit;
}

bool takeExti(unsigned line)
{
    const std::uint32_t bit = 1u << line;
    if ((reg(exti::Pd) & bit) == 0) {
        return false;
    }
    reg(exti::Pd) = bit;
    return true;
}

} // namespace dmm::hal::gd32f30x
