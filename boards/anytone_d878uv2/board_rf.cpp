// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tommi Tauriainen

#include "board_rf.h"

#include "pins.h"

#include "dmm/hal/gd32f30x/dac.h"
#include "dmm/hal/gd32f30x/gpio.h"
#include "dmm/hal/gd32f30x/pwm.h"

namespace board {

namespace hal = dmm::hal::gd32f30x;

namespace {

hal::Timer2Pwm tunePwm{ReceiveTuneChannel};

} // namespace

void BoardRf::configure()
{
    hal::write(TransmitEnable, false);
    hal::write(PowerAmplifierEnable, false);
    hal::configure(PowerAmplifierEnable, hal::PinMode::Output);
    hal::configure(PowerControl, hal::PinMode::Analog);
    hal::dac::start(PowerControlChannel, 0);
    hal::configure(ReceiveTune, hal::PinMode::Alternate, hal::PinSpeed::Medium);
    tunePwm.start(0, ReceiveTunePeriod);
}

void BoardRf::setReferenceTrim(std::uint16_t value)
{
    if (!m_trimStarted) {
        hal::configure(ReferenceTrim, hal::PinMode::Analog);
        hal::dac::start(ReferenceTrimChannel, value);
        m_trimStarted = true;
        return;
    }
    hal::dac::write(ReferenceTrimChannel, value);
}

void BoardRf::setReceiveTune(std::uint16_t value)
{
    tunePwm.setDuty(value > ReceiveTunePeriod ? ReceiveTunePeriod + 1 : value);
}

void BoardRf::setTransmitPath(bool on)
{
    hal::write(TransmitEnable, on);
}

void BoardRf::setPowerAmplifier(bool on)
{
    hal::write(PowerAmplifierEnable, on);
}

void BoardRf::setPowerControl(std::uint16_t value)
{
    hal::dac::write(PowerControlChannel, value);
}

} // namespace board
