// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Henri Lavikainen

#include "dmm/board/board_gps.h"

#include "pins.h"

#include "dmm/hal/gd32f30x/usart.h"

namespace board {

namespace hal = dmm::hal::gd32f30x;
using hal::PinMode;

namespace {

hal::Usart usart{GpsUsart};

} // namespace

void BoardGps::configure()
{
    hal::configure(GpsToReceiver, PinMode::InputPullDown);
    hal::configure(GpsFromReceiver, PinMode::InputPullDown);
}

void BoardGps::start()
{
    hal::configure(GpsFromReceiver, PinMode::InputPullUp);
    hal::configure(GpsToReceiver, PinMode::Alternate);
    usart.start(GpsBaud, GpsIrqPriority);
}

void BoardGps::stop()
{
    usart.stop();
    configure();
}

std::size_t BoardGps::read(std::span<std::uint8_t> data)
{
    return usart.read(data);
}

void BoardGps::write(std::span<const std::uint8_t> data)
{
    usart.write(data);
}

std::uint32_t BoardGps::lostBytes() const
{
    return usart.lostBytes();
}

void BoardGps::onInterrupt()
{
    usart.onInterrupt();
}

} // namespace board
