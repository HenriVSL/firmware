// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Henri Lavikainen

#include "dmm/board/board_rtc.h"

#include "dmm/hal/gd32f30x/rtc.h"

namespace board {

using dmm::hal::gd32f30x::Rtc;

bool BoardRtc::start()
{
    return Rtc::start() != Rtc::Start::NoCrystal;
}

std::uint32_t BoardRtc::seconds()
{
    return Rtc::seconds();
}

void BoardRtc::setSeconds(std::uint32_t utc)
{
    Rtc::setSeconds(utc);
}

} // namespace board
