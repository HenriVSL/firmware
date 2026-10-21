// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tommi Tauriainen

#include "dmm/hal/gd32f30x/parallel_lcd_bus.h"

#include "dmm/hal/gd32f30x/registers.h"

#include <initializer_list>

namespace dmm::hal::gd32f30x {

ParallelLcdBus::ParallelLcdBus(const Pins &pins)
    : m_pins(pins), m_dataSetReset(gpio::base(static_cast<unsigned>(pins.dataPort)) + gpio::Bop), m_writeSet(gpio::base(static_cast<unsigned>(pins.write.port)) + gpio::Bop), m_writeClear(gpio::base(static_cast<unsigned>(pins.write.port)) + gpio::Bc), m_writeMask(1u << pins.write.number)
{
}

void ParallelLcdBus::configurePins()
{
    for (Pin pin : {m_pins.chipSelect, m_pins.write, m_pins.dataCommand, m_pins.reset}) {
        gd32f30x::write(pin, true);
        configure(pin, PinMode::Output, PinSpeed::Fast);
    }
    for (std::uint8_t i = 0; i < 8; ++i) {
        const Pin pin{m_pins.dataPort, static_cast<std::uint8_t>(m_pins.firstDataPin + i)};
        gd32f30x::write(pin, false);
        configure(pin, PinMode::Output, PinSpeed::Fast);
    }
}

// One write cycle: all eight data lines in one store (bits to set in the low
// half, bits to clear in the high half), then a write strobe. The controller
// needs at least 15 ns low and 15 ns high; each store takes longer than that.
inline void ParallelLcdBus::writeByte(std::uint8_t value)
{
    const unsigned shift = m_pins.firstDataPin;
    reg(m_dataSetReset) = (static_cast<std::uint32_t>(value) << shift) | (static_cast<std::uint32_t>(~value & 0xFFu) << (shift + 16));
    reg(m_writeClear) = m_writeMask;
    asm volatile("nop");
    reg(m_writeSet) = m_writeMask;
}

void ParallelLcdBus::command(std::uint8_t code)
{
    gd32f30x::write(m_pins.chipSelect, false);
    gd32f30x::write(m_pins.dataCommand, false);
    writeByte(code);
    gd32f30x::write(m_pins.dataCommand, true);
}

void ParallelLcdBus::data(const std::uint8_t *bytes, std::size_t count)
{
    for (std::size_t i = 0; i < count; ++i) {
        writeByte(bytes[i]);
    }
}

void ParallelLcdBus::repeat(std::uint16_t pixel, std::uint32_t count)
{
    const auto high = static_cast<std::uint8_t>(pixel >> 8);
    const auto low = static_cast<std::uint8_t>(pixel);
    for (std::uint32_t i = 0; i < count; ++i) {
        writeByte(high);
        writeByte(low);
    }
}

void ParallelLcdBus::release()
{
    gd32f30x::write(m_pins.chipSelect, true);
}

void ParallelLcdBus::setReset(bool active)
{
    gd32f30x::write(m_pins.reset, !active);
}

} // namespace dmm::hal::gd32f30x
