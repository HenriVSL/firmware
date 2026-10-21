// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tommi Tauriainen

#include "dmm/hal/gd32f30x/parallel_host_port.h"

#include "dmm/hal/cortex_m/core.h"
#include "dmm/hal/gd32f30x/registers.h"

#include <initializer_list>

namespace dmm::hal::gd32f30x {

namespace {

// Strobe and settling times: well above what the peripheral needs (tens of
// nanoseconds), about 0.25 us each.
constexpr std::uint32_t StrobeCycles = 30;

} // namespace

ParallelHostPort::ParallelHostPort(const Pins &pins)
    : m_pins(pins)
{
}

void ParallelHostPort::configurePins()
{
    for (Pin pin : {m_pins.chipSelect, m_pins.write, m_pins.read}) {
        gd32f30x::write(pin, true);
        configure(pin, PinMode::Output, PinSpeed::Fast);
    }
    configure(m_pins.status, PinMode::InputPullUp);
    m_driving = true;
    driveData(false);
}

void ParallelHostPort::driveData(bool drive)
{
    if (drive == m_driving) {
        return;
    }
    for (std::uint8_t i = 0; i < 8; ++i) {
        const Pin pin{m_pins.dataPort, static_cast<std::uint8_t>(m_pins.firstDataPin + i)};
        configure(pin, drive ? PinMode::Output : PinMode::InputPullUp, PinSpeed::Fast);
    }
    m_driving = drive;
}

void ParallelHostPort::write(const std::uint8_t *bytes, std::size_t count)
{
    driveData(true);
    const unsigned shift = m_pins.firstDataPin;
    const std::uintptr_t dataSetClear = gpio::base(static_cast<unsigned>(m_pins.dataPort)) + gpio::Bop;
    gd32f30x::write(m_pins.chipSelect, false);
    for (std::size_t i = 0; i < count; ++i) {
        const std::uint32_t v = bytes[i];
        reg(dataSetClear) = (v << shift) | ((~v & 0xFFu) << (shift + 16));
        gd32f30x::write(m_pins.write, false);
        cortex_m::busyWaitCycles(StrobeCycles);
        gd32f30x::write(m_pins.write, true);
        cortex_m::busyWaitCycles(StrobeCycles);
    }
    gd32f30x::write(m_pins.chipSelect, true);
}

std::uint8_t ParallelHostPort::read()
{
    driveData(false);
    gd32f30x::write(m_pins.chipSelect, false);
    gd32f30x::write(m_pins.read, false);
    cortex_m::busyWaitCycles(StrobeCycles);
    const std::uint32_t port = reg(gpio::base(static_cast<unsigned>(m_pins.dataPort)) + gpio::Istat);
    gd32f30x::write(m_pins.read, true);
    gd32f30x::write(m_pins.chipSelect, true);
    cortex_m::busyWaitCycles(StrobeCycles);
    return static_cast<std::uint8_t>(port >> m_pins.firstDataPin);
}

void ParallelHostPort::release()
{
    for (std::uint8_t i = 0; i < 8; ++i) {
        configure({m_pins.dataPort, static_cast<std::uint8_t>(m_pins.firstDataPin + i)}, PinMode::Input);
    }
    configure(m_pins.status, PinMode::Input);
    m_driving = false;
}

bool ParallelHostPort::dataWaiting()
{
    return !gd32f30x::read(m_pins.status);
}

} // namespace dmm::hal::gd32f30x
