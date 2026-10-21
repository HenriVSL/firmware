// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tommi Tauriainen

#include "board_baseband.h"

#include "dmm/hal/cortex_m/core.h"
#include "dmm/hal/gd32f30x/clock.h"
#include "dmm/os/os.h"

#include <initializer_list>

namespace board {

namespace hal = dmm::hal::gd32f30x;
using hal::PinMode;

namespace {

void outputLow(Pin pin)
{
    hal::write(pin, false);
    hal::configure(pin, PinMode::Output);
}

} // namespace

void BoardBaseband::configurePins()
{
    m_port.release();
    for (Pin pin : {BasebandEnableFirst, BasebandEnableSecond, BasebandReset, BasebandWake, BasebandRestart, BasebandSleep,
             BasebandModeStrap, BasebandPort.chipSelect, BasebandPort.write, BasebandPort.read}) {
        outputLow(pin);
    }
}

void BoardBaseband::powerUpAndReset()
{
    // From any state, also a running chip: reset, power off and let it
    // drain, with every line low. The mode strap must be driven low again:
    // after start-up the chip uses that line itself.
    configurePins();
    dmm::os::sleepMs(BasebandOffMs);

    hal::write(BasebandEnableFirst, true);
    m_port.configurePins(); // strobes and select idle high, data lines in
    hal::write(BasebandEnableSecond, true);
    dmm::os::sleepMs(BasebandSettleMs);
    hal::write(BasebandReset, true);
    dmm::os::sleepMs(BasebandResetHighMs);
    hal::write(BasebandReset, false);
    dmm::os::sleepMs(BasebandResetLowMs);
    hal::write(BasebandReset, true); // the chip now waits for its boot program
    hal::configure(BasebandModeStrap, PinMode::InputPullDown);
}

void BoardBaseband::pollDelayUs(unsigned us)
{
    static unsigned busyUs = 0;
    busyUs += us;
    if (busyUs >= 1000) {
        busyUs = 0;
        dmm::os::sleepMs(1);
        return;
    }
    dmm::hal::cortex_m::busyWaitCycles(us * (hal::SystemClockHz / 1'000'000));
}

} // namespace board
