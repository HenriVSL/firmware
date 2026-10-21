// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tommi Tauriainen

#pragma once

#include "board_adc.h"
#include "pins.h"

#include "dmm/hal/gd32f30x/gpio.h"
#include "dmm/services/battery.h"

namespace board {

class BoardBattery final : public dmm::services::BatterySensor
{
public:
    void configure() { dmm::hal::gd32f30x::configure(BatterySense, dmm::hal::gd32f30x::PinMode::Analog); }

    int readMillivolts() override
    {
        return static_cast<int>(adc().read(BatteryChannel)) * BatteryFullScaleMv / 4095;
    }
};

} // namespace board
