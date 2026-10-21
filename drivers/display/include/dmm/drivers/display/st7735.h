// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tommi Tauriainen

#pragma once

#include "dmm/hal/lcd_bus.h"
#include "dmm/lib/fn_ref.h"

#include <cstdint>
#include <initializer_list>

namespace dmm::drivers::display {

/// Sitronix ST7735 TFT controller in 16-bit RGB565 mode, written directly
/// (no frame buffer). Command codes from the ST7735 datasheet.
class St7735
{
public:
    struct Geometry
    {
        int width; // as seen by the user
        int height;
        /// The panel is mounted turned: screen x runs along the controller's
        /// rows and screen y along its columns.
        bool axesSwapped;
        /// Memory access control byte (mirroring, colour order).
        std::uint8_t memoryAccess;
    };

    St7735(hal::LcdBus &bus, const Geometry &geometry);

    /// Hardware reset, then the controller out of sleep in RGB565 with the
    /// display on. Its other settings keep their power-on defaults.
    void init(lib::FnRef<void(unsigned)> sleepMs);

    /// Sleep keeps the picture memory and blanks the panel.
    void setSleep(bool asleep, lib::FnRef<void(unsigned)> sleepMs);

    int width() const { return m_geometry.width; }
    int height() const { return m_geometry.height; }

    void fillRect(int x, int y, int w, int h, std::uint16_t color);
    void drawBitmap(int x, int y, int w, int h, const std::uint8_t *rows, int stride, std::uint16_t fg, std::uint16_t bg);

private:
    struct Rect
    {
        int x0, y0, x1, y1; // inclusive
    };

    bool clip(int x, int y, int w, int h, Rect &r) const;
    void beginWrite(const Rect &r);
    void sendCommand(std::uint8_t code, std::initializer_list<std::uint8_t> parameters = {});

    hal::LcdBus &m_bus;
    Geometry m_geometry;
};

} // namespace dmm::drivers::display
