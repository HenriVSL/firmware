// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tommi Tauriainen

#pragma once

#include "dmm/hal/gd32f30x/parallel_lcd_bus.h"
#include "dmm/os/os.h"

namespace board {

/// The display bus. Its data lines are also the keypad rows, so the
/// display and the keypad scan each hold panelPinsLock() while they use them.
dmm::hal::gd32f30x::ParallelLcdBus &lcdBus();
dmm::os::Mutex &panelPinsLock();

class PanelPinsGuard
{
public:
    PanelPinsGuard() { panelPinsLock().lock(); }
    ~PanelPinsGuard() { panelPinsLock().unlock(); }
    PanelPinsGuard(const PanelPinsGuard &) = delete;
    PanelPinsGuard &operator=(const PanelPinsGuard &) = delete;
};

} // namespace board
