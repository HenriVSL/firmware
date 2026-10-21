// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tommi Tauriainen

#pragma once

#include "dmm/drivers/display/st7735.h"
#include "dmm/hal/gd32f30x/pwm.h"
#include "dmm/services/display.h"

namespace board {

/// The 160 x 128 colour display and the backlight it shares with the keypad.
class BoardDisplay final : public dmm::services::Display
{
public:
    /// @p panelStrap: the variant strap read at power-on; picks the backlight
    /// levels.
    explicit BoardDisplay(bool panelStrap);

    void start() override;
    int width() const override { return m_panel.width(); }
    int height() const override { return m_panel.height(); }
    void fillRect(int x, int y, int w, int h, dmm::services::Color color) override;
    void drawBitmap(int x, int y, int w, int h, const std::uint8_t *rows, int stride, dmm::services::Color fg,
        dmm::services::Color bg) override;
    void setBacklight(int level) override;
    void setPanelOn(bool on) override;

private:
    bool m_strap;
    dmm::drivers::display::St7735 m_panel;
    dmm::hal::gd32f30x::Timer7ComplementaryPwm m_pwm;
};

} // namespace board
