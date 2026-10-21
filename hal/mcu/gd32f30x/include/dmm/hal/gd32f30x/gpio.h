// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tommi Tauriainen

#pragma once

#include <cstdint>

namespace dmm::hal::gd32f30x {

enum class Port : std::uint8_t
{
    A,
    B,
    C,
    D,
    E,
};

struct Pin
{
    Port port;
    std::uint8_t number;
};

enum class PinMode : std::uint8_t
{
    Analog,
    Input,
    InputPullUp,
    InputPullDown,
    Output,
    OutputOpenDrain,
    Alternate,
    AlternateOpenDrain,
};

/// Output edge rate. Slower edges radiate less; buses need Fast.
enum class PinSpeed : std::uint8_t
{
    Slow,   // 2 MHz
    Medium, // 10 MHz
    Fast,   // 50 MHz
};

void enablePortClock(Port port);

/// Sets a pin's mode. Not safe against another task configuring a pin of
/// the same port at the same time; boards configure pins at start-up.
void configure(Pin pin, PinMode mode, PinSpeed speed = PinSpeed::Slow);

/// Drives an output high or low. Safe from any task or interrupt.
void write(Pin pin, bool high);

bool read(Pin pin);

/// Drives the pins in @p high high and those in @p low low, all in one
/// store. Bit n = pin n of @p port.
void writePins(Port port, std::uint16_t high, std::uint16_t low);

/// Turns the JTAG and SWD debug port off, so PA13, PA14, PA15, PB3 and PB4
/// become plain GPIO. A debugger can then attach only while the MCU is
/// held in reset.
void releaseDebugPins();

/// Turns only JTAG off: PA15, PB3 and PB4 become GPIO, SWD stays on
/// PA13/PA14.
void releaseJtagPins();

} // namespace dmm::hal::gd32f30x
