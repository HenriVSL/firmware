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
    void frameDone() override;
    /// For tests, the drawing sent over the console as it happens, as
    /// "scr:" lines a PC puts together:
    ///   stream on|off  from now on, until off
    ///   watch <s>      for @p s seconds
    /// Lines: "start <width> <height>", "f <n> <x> <y> <w> <h> <colour>",
    /// "b <n> <x> <y> <w> <h> <fg> <bg> <rows>" (each row's bytes in hex,
    /// leftmost pixel in the top bit), "e <n>" after each UI pass that drew,
    /// and "stop <n>"; n counts the lines, so a lost one shows. Colours are
    /// RGB565. Drawing waits for room in the log rather than lose a line.
    bool debugCommand(std::string_view text) override;

private:
    bool m_strap;
    void stream(bool on);
    void waitForLogRoom();
    // Streaming: on, until when (0 = until off), the next line's number,
    // and whether the pass drew anything.
    bool m_streaming = false;
    std::uint32_t m_streamUntil = 0;
    std::uint16_t m_streamLine = 0;
    bool m_drew = false;
    dmm::drivers::display::St7735 m_panel;
    dmm::hal::gd32f30x::Timer7ComplementaryPwm m_pwm;
};

} // namespace board
