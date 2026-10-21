// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tommi Tauriainen

#include "dmm/hal/gd32f30x/three_wire_bus.h"

#include "dmm/hal/cortex_m/core.h"
#include "dmm/hal/gd32f30x/clock.h"

namespace dmm::hal::gd32f30x {

namespace {

// Half a clock period: 1 us, far slower than the chip needs.
constexpr std::uint32_t HalfPeriodCycles = SystemClockHz / 1'000'000;

void halfPeriod()
{
    cortex_m::busyWaitCycles(HalfPeriodCycles);
}

} // namespace

ThreeWireBus::ThreeWireBus(const Pins &pins)
    : m_pins(pins)
{
}

void ThreeWireBus::configurePins()
{
    gd32f30x::write(m_pins.enable, true);
    configure(m_pins.enable, PinMode::Output);
    gd32f30x::write(m_pins.clock, false);
    configure(m_pins.clock, PinMode::Output);
    gd32f30x::write(m_pins.data, false);
    configure(m_pins.data, PinMode::Output);
}

void ThreeWireBus::sendByte(std::uint8_t byte)
{
    for (int bit = 0; bit < 8; ++bit) {
        gd32f30x::write(m_pins.data, (byte & 0x80u) != 0);
        halfPeriod();
        gd32f30x::write(m_pins.clock, true);
        halfPeriod();
        gd32f30x::write(m_pins.clock, false);
        byte = static_cast<std::uint8_t>(byte << 1);
    }
}

void ThreeWireBus::write(std::uint8_t address, std::uint16_t value)
{
    gd32f30x::write(m_pins.enable, false);
    sendByte(address & 0x7Fu);
    sendByte(static_cast<std::uint8_t>(value >> 8));
    sendByte(static_cast<std::uint8_t>(value));
    halfPeriod();
    gd32f30x::write(m_pins.enable, true);
    gd32f30x::write(m_pins.data, false);
}

std::uint16_t ThreeWireBus::read(std::uint8_t address)
{
    gd32f30x::write(m_pins.enable, false);
    sendByte(address | 0x80u);
    gd32f30x::write(m_pins.data, true); // pull-up while the chip drives the line
    configure(m_pins.data, PinMode::InputPullUp);
    std::uint16_t value = 0;
    for (int bit = 0; bit < 16; ++bit) {
        halfPeriod();
        gd32f30x::write(m_pins.clock, true);
        halfPeriod();
        value = static_cast<std::uint16_t>((value << 1) | (gd32f30x::read(m_pins.data) ? 1u : 0u));
        gd32f30x::write(m_pins.clock, false);
    }
    gd32f30x::write(m_pins.data, false);
    configure(m_pins.data, PinMode::Output);
    halfPeriod();
    gd32f30x::write(m_pins.enable, true);
    return value;
}

} // namespace dmm::hal::gd32f30x
