// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tommi Tauriainen

#include "dmm/hal/gd32f30x/gpio.h"

#include "dmm/hal/cortex_m/core.h"
#include "dmm/hal/gd32f30x/registers.h"

namespace dmm::hal::gd32f30x {

namespace {

std::uintptr_t portBase(Port port)
{
    return gpio::base(static_cast<unsigned>(port));
}

// Each pin has a 4-bit field: bits 1:0 select input (0) or an output edge
// rate, bits 3:2 the input or output kind.
std::uint32_t modeBits(PinMode mode, PinSpeed speed)
{
    const std::uint32_t rate = speed == PinSpeed::Fast ? 3u : speed == PinSpeed::Medium ? 1u
                                                                                        : 2u;
    switch (mode) {
    case PinMode::Analog:
        return 0x0;
    case PinMode::Input:
        return 0x4;
    case PinMode::InputPullUp:
    case PinMode::InputPullDown:
        return 0x8;
    case PinMode::Output:
        return rate;
    case PinMode::OutputOpenDrain:
        return 0x4 | rate;
    case PinMode::Alternate:
        return 0x8 | rate;
    case PinMode::AlternateOpenDrain:
        return 0xC | rate;
    }
    return 0x4;
}

} // namespace

void enablePortClock(Port port)
{
    reg(rcu::Apb2En) |= rcu::gpioClock(static_cast<unsigned>(port));
    // Reading back makes sure the clock is on before the port is touched.
    [[maybe_unused]] const std::uint32_t enabled = reg(rcu::Apb2En);
}

void configure(Pin pin, PinMode mode, PinSpeed speed)
{
    const std::uintptr_t base = portBase(pin.port);
    // The pull direction of an input with pull comes from the output latch.
    if (mode == PinMode::InputPullUp) {
        reg(base + gpio::Bop) = 1u << pin.number;
    } else if (mode == PinMode::InputPullDown) {
        reg(base + gpio::Bc) = 1u << pin.number;
    }
    volatile std::uint32_t &ctl = reg(base + (pin.number < 8 ? gpio::Ctl0 : gpio::Ctl1));
    const unsigned shift = 4u * (pin.number % 8u);
    // Eight pins share the register and tasks change pin modes at run time,
    // so the read-modify-write must not be interrupted.
    const cortex_m::InterruptLock lock;
    ctl = (ctl & ~(0xFu << shift)) | (modeBits(mode, speed) << shift);
}

void write(Pin pin, bool high)
{
    reg(portBase(pin.port) + (high ? gpio::Bop : gpio::Bc)) = 1u << pin.number;
}

void writePins(Port port, std::uint16_t high, std::uint16_t low)
{
    reg(portBase(port) + gpio::Bop) = high | (static_cast<std::uint32_t>(low) << 16);
}

bool read(Pin pin)
{
    return (reg(portBase(pin.port) + gpio::Istat) & (1u << pin.number)) != 0;
}

void releaseDebugPins()
{
    reg(rcu::Apb2En) |= rcu::AfioClock;
    [[maybe_unused]] const std::uint32_t enabled = reg(rcu::Apb2En);
    // The SWJ field reads back undefined: write it with the other bits kept.
    reg(afio::Pcf0) = (reg(afio::Pcf0) & ~afio::SwjMask) | afio::SwjAllOff;
}

void releaseJtagPins()
{
    reg(rcu::Apb2En) |= rcu::AfioClock;
    [[maybe_unused]] const std::uint32_t enabled = reg(rcu::Apb2En);
    reg(afio::Pcf0) = (reg(afio::Pcf0) & ~afio::SwjMask) | afio::SwjSwdOnly;
}

} // namespace dmm::hal::gd32f30x
