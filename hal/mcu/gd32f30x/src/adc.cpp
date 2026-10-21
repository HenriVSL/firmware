// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tommi Tauriainen

#include "dmm/hal/gd32f30x/adc.h"

#include "dmm/hal/cortex_m/core.h"
#include "dmm/hal/gd32f30x/clock.h"
#include "dmm/hal/gd32f30x/registers.h"

namespace dmm::hal::gd32f30x {

namespace {

volatile std::uint32_t &adcReg(std::uintptr_t offset)
{
    return reg(adc::Adc0 + offset);
}

// Bounds every wait on the ADC, so a fault cannot hang the caller.
constexpr std::uint32_t MaxPolls = 100'000;

void waitWhileSet(std::uint32_t bit)
{
    for (std::uint32_t i = 0; i < MaxPolls && (adcReg(adc::Ctl1) & bit) != 0; ++i) {
    }
}

} // namespace

void Adc0::start()
{
    reg(rcu::Cfg0) = (reg(rcu::Cfg0) & ~rcu::AdcPrescalerMask) | rcu::AdcApb2Div8;
    reg(rcu::Apb2En) |= rcu::Adc0Clock;
    [[maybe_unused]] const std::uint32_t enabled = reg(rcu::Apb2En);

    // Every channel gets the longest sample time: the knob and battery
    // dividers have high source impedance.
    std::uint32_t all = 0;
    for (unsigned i = 0; i < 10; ++i) {
        all |= adc::LongestSample << (3 * i);
    }
    adcReg(adc::Sampt0) = all & 0x00FFFFFFu;
    adcReg(adc::Sampt1) = all;
    adcReg(adc::Rsq0) = 0; // one conversion per sequence

    adcReg(adc::Ctl1) = adc::On | adc::TriggerEnable | adc::SoftwareTrigger;
    cortex_m::busyWaitCycles(SystemClockHz / 100'000); // 10 us to power up
    adcReg(adc::Ctl1) |= adc::ResetCalibration;
    waitWhileSet(adc::ResetCalibration);
    adcReg(adc::Ctl1) |= adc::Calibrate;
    waitWhileSet(adc::Calibrate);
}

std::uint16_t Adc0::read(unsigned channel)
{
    adcReg(adc::Rsq2) = channel & 0x1Fu;
    adcReg(adc::Stat) = 0;
    adcReg(adc::Ctl1) |= adc::SoftwareStart;
    for (std::uint32_t i = 0; i < MaxPolls && (adcReg(adc::Stat) & adc::EndOfConversion) == 0; ++i) {
    }
    return static_cast<std::uint16_t>(adcReg(adc::Rdata) & 0xFFFu);
}

} // namespace dmm::hal::gd32f30x
