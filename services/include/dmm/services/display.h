// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tommi Tauriainen

#pragma once

#include "dmm/services/font.h"

#include <cstdint>
#include <string_view>

namespace dmm::services {

/// RGB565 colour.
using Color = std::uint16_t;

constexpr Color rgb(unsigned r, unsigned g, unsigned b)
{
    return static_cast<Color>(((r & 0xF8u) << 8) | ((g & 0xFCu) << 3) | (b >> 3));
}

namespace colors {
inline constexpr Color Black = rgb(0, 0, 0);
inline constexpr Color White = rgb(255, 255, 255);
inline constexpr Color Red = rgb(255, 0, 0);
inline constexpr Color Green = rgb(0, 255, 0);
inline constexpr Color Blue = rgb(0, 0, 255);
inline constexpr Color Yellow = rgb(255, 255, 0);
inline constexpr Color Cyan = rgb(0, 255, 255);
inline constexpr Color Magenta = rgb(255, 0, 255);
inline constexpr Color Grey = rgb(128, 128, 128);
} // namespace colors

/// A colour display without a frame buffer: every call goes straight to
/// the panel. Coordinates are in pixels from the top left; anything outside
/// the panel is clipped. Used by one task only.
class Display
{
public:
    /// Brings the panel up (blank, backlight off). Call once, from the task
    /// that owns the display.
    virtual void start() = 0;

    virtual int width() const = 0;
    virtual int height() const = 0;

    virtual void fillRect(int x, int y, int w, int h, Color color) = 0;

    /// Draws a 1-bit image: @p h rows of @p stride bytes, most significant
    /// bit leftmost; set bits in @p fg, clear bits in @p bg.
    virtual void drawBitmap(int x, int y, int w, int h, const std::uint8_t *rows, int stride, Color fg, Color bg) = 0;

    /// Backlight level: 0 = off, 1..BacklightLevels = dimmest..brightest.
    static constexpr int BacklightLevels = 5;
    virtual void setBacklight(int level) = 0;

    /// Panel sleep: off keeps the picture memory but blanks the screen.
    virtual void setPanelOn(bool on) = 0;

    /// The drawing for one change of the screen is complete.
    virtual void frameDone() {}

    /// The display's console commands, answered in the log; false when
    /// @p text is none of them.
    virtual bool debugCommand(std::string_view text)
    {
        (void)text;
        return false;
    }

protected:
    ~Display() = default;
};

/// Draws UTF-8 @p text with its top left at (@p x, @p y) and returns the x
/// after it. Characters the font lacks show as '?'.
int drawText(Display &display, const Font &font, int x, int y, std::string_view text, Color fg, Color bg);

} // namespace dmm::services
