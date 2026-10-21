// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tommi Tauriainen

#include "dmm/hal/gd32f30x/pwm.h"

#include "dmm/hal/gd32f30x/registers.h"

namespace dmm::hal::gd32f30x {

namespace {

volatile std::uint32_t &timerReg(std::uintptr_t offset)
{
    return reg(timer::Timer7 + offset);
}

} // namespace

Timer7ComplementaryPwm::Timer7ComplementaryPwm(unsigned channel)
    : m_channel(channel)
{
}

void Timer7ComplementaryPwm::start(std::uint16_t prescaler, std::uint16_t period)
{
    reg(rcu::Apb2En) |= rcu::Timer7Clock | rcu::AfioClock;
    [[maybe_unused]] const std::uint32_t enabled = reg(rcu::Apb2En);

    timerReg(timer::Ctl0) = 0;
    timerReg(timer::Psc) = prescaler;
    timerReg(timer::Car) = period;
    timerReg(timer::channelValue(m_channel)) = 0;

    // PWM mode 0: the reference is high while the counter is below the
    // compare value. Only the complementary output is enabled, and then it
    // carries the reference XOR its polarity bit: polarity 0 makes the pin
    // high for the duty time.
    const unsigned shift = 8 * m_channel;
    timerReg(timer::Chctl0) = (timerReg(timer::Chctl0) & ~(0xFFu << shift)) |
        ((timer::PwmMode0 | timer::CompareShadow) << shift);
    timerReg(timer::Chctl2) = (timerReg(timer::Chctl2) & ~(0xFu << (4 * m_channel))) |
        timer::complementaryEnable(m_channel);
    timerReg(timer::Cchp) = timer::PrimaryOutputEnable;
    timerReg(timer::Swevg) = timer::UpdateEvent;
    timerReg(timer::Ctl0) = timer::AutoReloadShadow | timer::CounterEnable;
}

void Timer7ComplementaryPwm::setDuty(std::uint16_t duty)
{
    timerReg(timer::channelValue(m_channel)) = duty;
}

Timer2Pwm::Timer2Pwm(unsigned channel)
    : m_channel(channel)
{
}

void Timer2Pwm::start(std::uint16_t prescaler, std::uint16_t period)
{
    reg(rcu::Apb1En) |= rcu::Timer2Clock;
    [[maybe_unused]] const std::uint32_t enabled = reg(rcu::Apb1En);
    auto timer2 = [](std::uintptr_t offset)->volatile std::uint32_t &
    {
        return reg(timer::Timer2 + offset);
    };

    timer2(timer::Ctl0) = 0;
    timer2(timer::Psc) = prescaler;
    timer2(timer::Car) = period;
    timer2(timer::channelValue(m_channel)) = 0;
    // PWM mode 0 with polarity 0: the pin is high while the counter is
    // below the compare value.
    const std::uintptr_t chctl = m_channel < 2 ? timer::Chctl0 : timer::Chctl1;
    const unsigned shift = 8 * (m_channel % 2);
    timer2(chctl) = (timer2(chctl) & ~(0xFFu << shift)) | ((timer::PwmMode0 | timer::CompareShadow) << shift);
    timer2(timer::Chctl2) = (timer2(timer::Chctl2) & ~(0xFu << (4 * m_channel))) | timer::channelEnable(m_channel);
    timer2(timer::Swevg) = timer::UpdateEvent;
    timer2(timer::Ctl0) = timer::AutoReloadShadow | timer::CounterEnable;
}

void Timer2Pwm::setDuty(std::uint16_t duty)
{
    reg(timer::Timer2 + timer::channelValue(m_channel)) = duty;
}

} // namespace dmm::hal::gd32f30x
