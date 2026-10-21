// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tommi Tauriainen

#pragma once

#include <cstdint>

namespace dmm::hal::gd32f30x {

/// PWM on the complementary output of TIMER7 channel 0 or 1 (from the
/// 120 MHz APB2 timer clock), driven low for @c duty / (@c period + 1) of
/// each cycle: suits a switch that is on while its pin is low. The board
/// sets the pin to alternate function.
class Timer7ComplementaryPwm
{
public:
    explicit Timer7ComplementaryPwm(unsigned channel);

    /// Starts with the output always low (duty 0).
    void start(std::uint16_t prescaler, std::uint16_t period);

    /// High time in timer counts, 0..period+1. Takes effect next cycle.
    void setDuty(std::uint16_t duty);

private:
    unsigned m_channel;
};

/// PWM on TIMER2 channel 0..3 (from the 120 MHz APB1 timer clock), the
/// pin high for @c duty / (@c period + 1) of each cycle. The board sets the
/// pin to alternate function.
class Timer2Pwm
{
public:
    explicit Timer2Pwm(unsigned channel);

    /// Starts with the output always low (duty 0).
    void start(std::uint16_t prescaler, std::uint16_t period);

    /// High time in timer counts, 0..period+1. Takes effect next cycle.
    void setDuty(std::uint16_t duty);

private:
    unsigned m_channel;
};

} // namespace dmm::hal::gd32f30x
