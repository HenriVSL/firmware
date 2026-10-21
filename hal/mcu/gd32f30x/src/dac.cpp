// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tommi Tauriainen

#include "dmm/hal/gd32f30x/dac.h"

#include "dmm/hal/gd32f30x/registers.h"

namespace dmm::hal::gd32f30x::dac {

namespace {

constexpr std::uint16_t Max = 0xFFF;

} // namespace

void start(unsigned channel, std::uint16_t value)
{
    reg(rcu::Apb1En) |= rcu::DacClock;
    [[maybe_unused]] const std::uint32_t enabled = reg(rcu::Apb1En);
    write(channel, value);
    // Unbuffered: the outputs drive high-impedance control inputs.
    reg(dac::Ctl) |= bufferOff(channel) | enable(channel);
}

void write(unsigned channel, std::uint16_t value)
{
    reg(data12(channel)) = value > Max ? Max : value;
}

} // namespace dmm::hal::gd32f30x::dac
