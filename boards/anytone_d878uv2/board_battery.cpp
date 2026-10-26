// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tommi Tauriainen

#include "dmm/board/board_battery.h"

#include "board_adc.h"
#include "pins.h"

#include "dmm/hal/gd32f30x/gpio.h"

namespace board {

void BoardBattery::configure()
{
    dmm::hal::gd32f30x::configure(BatterySense, dmm::hal::gd32f30x::PinMode::Analog);
}

int BoardBattery::readMillivolts()
{
    return static_cast<int>(adc().read(BatteryChannel)) * BatteryFullScaleMv / 4095;
}

dmm::services::BatteryThresholds BoardBattery::thresholds() const
{
    return {{6450, 6850, 7450}, 100, 6000, 6100, 5950, 10'000};
}

} // namespace board
