// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tommi Tauriainen

#include "board_display.h"

#include "panel_bus.h"
#include "pins.h"

#include "dmm/services/log.h"

namespace board {

namespace hal = dmm::hal::gd32f30x;

namespace {

void sleepMs(unsigned ms)
{
    dmm::os::sleepMs(ms);
}

} // namespace

// Mounted turned: screen x runs along the controller's rows, which run
// mirrored; colours in BGR order.
BoardDisplay::BoardDisplay(bool panelStrap)
    : m_strap(panelStrap), m_panel(lcdBus(), {160, 128, true, 0x88}), m_pwm(BacklightChannel)
{
}

void BoardDisplay::start()
{
    dmm::services::log::info("display") << "panel strap " << int{m_strap};

    hal::write(Backlight, false); // dark until the PWM takes over
    hal::configure(Backlight, hal::PinMode::Output);
    m_pwm.start(BacklightPrescaler, BacklightPeriod);
    hal::configure(Backlight, hal::PinMode::Alternate, hal::PinSpeed::Medium);

    PanelPinsGuard guard;
    m_panel.init(sleepMs);
}

void BoardDisplay::fillRect(int x, int y, int w, int h, dmm::services::Color color)
{
    PanelPinsGuard guard;
    m_panel.fillRect(x, y, w, h, color);
}

void BoardDisplay::drawBitmap(int x, int y, int w, int h, const std::uint8_t *rows, int stride,
    dmm::services::Color fg, dmm::services::Color bg)
{
    PanelPinsGuard guard;
    m_panel.drawBitmap(x, y, w, h, rows, stride, fg, bg);
}

void BoardDisplay::setBacklight(int level)
{
    level = level < 0 ? 0 : level > BacklightLevels ? BacklightLevels
                                                    : level;
    m_pwm.setDuty(m_strap ? BacklightDutyStrap1[level] : BacklightDutyStrap0[level]);
}

void BoardDisplay::setPanelOn(bool on)
{
    PanelPinsGuard guard;
    m_panel.setSleep(!on, sleepMs);
}

} // namespace board
