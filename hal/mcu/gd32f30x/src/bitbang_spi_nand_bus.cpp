// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tommi Tauriainen

#include "dmm/hal/gd32f30x/bitbang_spi_nand_bus.h"

namespace dmm::hal::gd32f30x {

namespace {

// The chip drives its outputs after the falling clock edge; a few cycles
// pass before they are sampled.
inline void settle()
{
    asm volatile("nop\n\tnop\n\tnop\n\tnop");
}

} // namespace

BitBangSpiNandBus::BitBangSpiNandBus(const Pins &pins)
    : m_pins(pins)
{
}

void BitBangSpiNandBus::configurePins()
{
    gd32f30x::write(m_pins.chipSelect, true);
    configure(m_pins.chipSelect, PinMode::Output, PinSpeed::Fast);
    gd32f30x::write(m_pins.clock, true);
    configure(m_pins.clock, PinMode::Output, PinSpeed::Fast);
    gd32f30x::write(m_pins.dataIn, true);
    configure(m_pins.dataIn, PinMode::Output, PinSpeed::Fast);
    configure(m_pins.dataOut, PinMode::InputPullUp);
}

void BitBangSpiNandBus::select()
{
    gd32f30x::write(m_pins.chipSelect, false);
}

void BitBangSpiNandBus::deselect()
{
    gd32f30x::write(m_pins.chipSelect, true);
}

void BitBangSpiNandBus::write(const std::uint8_t *bytes, std::size_t count)
{
    for (std::size_t i = 0; i < count; ++i) {
        std::uint8_t b = bytes[i];
        for (int bit = 0; bit < 8; ++bit) {
            gd32f30x::write(m_pins.clock, false);
            gd32f30x::write(m_pins.dataIn, (b & 0x80u) != 0);
            gd32f30x::write(m_pins.clock, true);
            b = static_cast<std::uint8_t>(b << 1);
        }
    }
}

void BitBangSpiNandBus::read(std::uint8_t *bytes, std::size_t count)
{
    for (std::size_t i = 0; i < count; ++i) {
        unsigned b = 0;
        for (int bit = 0; bit < 8; ++bit) {
            gd32f30x::write(m_pins.clock, false);
            settle();
            b = (b << 1) | (gd32f30x::read(m_pins.dataOut) ? 1u : 0u);
            gd32f30x::write(m_pins.clock, true);
        }
        bytes[i] = static_cast<std::uint8_t>(b);
    }
}

// Each clock carries two bits, the higher one on the chip's data-out line.
void BitBangSpiNandBus::readDual(std::uint8_t *bytes, std::size_t count)
{
    configure(m_pins.dataIn, PinMode::InputPullUp);
    for (std::size_t i = 0; i < count; ++i) {
        unsigned b = 0;
        for (int pair = 0; pair < 4; ++pair) {
            gd32f30x::write(m_pins.clock, false);
            settle();
            b = (b << 2) | (gd32f30x::read(m_pins.dataOut) ? 2u : 0u) | (gd32f30x::read(m_pins.dataIn) ? 1u : 0u);
            gd32f30x::write(m_pins.clock, true);
        }
        bytes[i] = static_cast<std::uint8_t>(b);
    }
    configure(m_pins.dataIn, PinMode::Output, PinSpeed::Fast);
}

} // namespace dmm::hal::gd32f30x
