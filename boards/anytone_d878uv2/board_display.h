// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tommi Tauriainen

#pragma once

#include "dmm/drivers/display/st7735.h"
#include "dmm/hal/gd32f30x/pwm.h"
#include "dmm/services/display.h"
#include "dmm/services/screen_mirror.h"

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
    void frameDone() override { m_mirror.endFrame(); }
    /// For tests, a copy of the screen read over the console:
    ///   rows <first> <count>  each row as hexadecimal colour indices, two
    ///                         lines of 80 pixels, low nibble left
    ///   colours               the colours the indices stand for (RGB565)
    ///   flicker               pixels that changed colour more than once in
    ///                         one frame since the last ask, and the worst
    ///                         frame
    bool debugCommand(std::string_view text) override;

private:
    bool m_strap;
    dmm::services::ScreenMirror<160, 128> m_mirror;
    dmm::drivers::display::St7735 m_panel;
    dmm::hal::gd32f30x::Timer7ComplementaryPwm m_pwm;
};

} // namespace board
