// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Henri Lavikainen

#pragma once

#include "dmm/services/display.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace dmm::services {

/// A copy of what has been drawn on a W x H display, for tests: four bits a
/// pixel indexing a table of up to 16 colours, filled as colours first
/// appear (later ones share the nearest entry). Also counts flicker: pixels
/// that change colour more than once within one frame, between two calls
/// to endFrame().
template <int W, int H>
class ScreenMirror
{
public:
    static constexpr int Width = W;
    static constexpr int Height = H;
    static constexpr std::size_t Colours = 16;

    void fill(int x, int y, int w, int h, Color color)
    {
        const std::uint8_t c = index(color);
        clip(x, y, w, h);
        for (int row = y; row < y + h; ++row) {
            for (int col = x; col < x + w; ++col) {
                set(col, row, c);
            }
        }
    }

    void bitmap(int x, int y, int w, int h, const std::uint8_t *rows, int stride, Color fg, Color bg)
    {
        const std::uint8_t f = index(fg);
        const std::uint8_t b = index(bg);
        for (int r = 0; r < h; ++r) {
            for (int c = 0; c < w; ++c) {
                const int col = x + c;
                const int row = y + r;
                if (col < 0 || row < 0 || col >= W || row >= H) {
                    continue;
                }
                const bool on = (rows[r * stride + c / 8] >> (7 - c % 8) & 1) != 0;
                set(col, row, on ? f : b);
            }
        }
    }

    /// Ends a frame: its flicker is added to the count and the next frame
    /// starts clean.
    void endFrame()
    {
        if (m_frameFlicker > m_worstFrame) {
            m_worstFrame = m_frameFlicker;
        }
        m_flicker += m_frameFlicker;
        m_frameFlicker = 0;
        m_changed.fill(0);
    }

    /// Pixel (@p x, @p y) as an index into colours().
    std::uint8_t at(int x, int y) const
    {
        const std::size_t i = static_cast<std::size_t>(y * W + x);
        return static_cast<std::uint8_t>(m_pixels[i / 2] >> (i % 2 ? 4 : 0) & 0xF);
    }
    /// Two pixels of row @p y from @p x (even): low nibble left.
    std::uint8_t pair(int x, int y) const { return m_pixels[static_cast<std::size_t>(y * W + x) / 2]; }
    const std::array<Color, Colours> &colours() const { return m_colours; }
    std::size_t colourCount() const { return m_colourCount; }

    /// Flicker counted since the last call, and the worst single frame in
    /// that time; both start again from 0.
    std::uint32_t takeFlicker(std::uint32_t &worstFrame)
    {
        const std::uint32_t n = m_flicker;
        worstFrame = m_worstFrame;
        m_flicker = 0;
        m_worstFrame = 0;
        return n;
    }

private:
    void clip(int &x, int &y, int &w, int &h) const
    {
        if (x < 0) {
            w += x;
            x = 0;
        }
        if (y < 0) {
            h += y;
            y = 0;
        }
        if (x + w > W) {
            w = W - x;
        }
        if (y + h > H) {
            h = H - y;
        }
        if (w < 0 || h < 0) {
            w = h = 0;
        }
    }

    std::uint8_t index(Color color)
    {
        for (std::size_t i = 0; i < m_colourCount; ++i) {
            if (m_colours[i] == color) {
                return static_cast<std::uint8_t>(i);
            }
        }
        if (m_colourCount < Colours) {
            m_colours[m_colourCount] = color;
            return static_cast<std::uint8_t>(m_colourCount++);
        }
        // The nearest colour in the table, by the sum of the channel
        // differences.
        auto channels = [](Color c, int &r, int &g, int &b) {
            r = c >> 11;
            g = (c >> 5) & 0x3F;
            b = c & 0x1F;
        };
        int r, g, b;
        channels(color, r, g, b);
        std::size_t best = 0;
        int bestDistance = 1 << 30;
        for (std::size_t i = 0; i < Colours; ++i) {
            int r2, g2, b2;
            channels(m_colours[i], r2, g2, b2);
            const int d = (r > r2 ? r - r2 : r2 - r) * 2 + (g > g2 ? g - g2 : g2 - g) + (b > b2 ? b - b2 : b2 - b) * 2;
            if (d < bestDistance) {
                bestDistance = d;
                best = i;
            }
        }
        return static_cast<std::uint8_t>(best);
    }

    void set(int x, int y, std::uint8_t c)
    {
        const std::size_t i = static_cast<std::size_t>(y * W + x);
        std::uint8_t &byte = m_pixels[i / 2];
        const int shift = i % 2 ? 4 : 0;
        if ((byte >> shift & 0xF) == c) {
            return;
        }
        byte = static_cast<std::uint8_t>((byte & ~(0xF << shift)) | c << shift);
        std::uint8_t &bit = m_changed[i / 8];
        const std::uint8_t mask = static_cast<std::uint8_t>(1u << (i % 8));
        if ((bit & mask) != 0) {
            ++m_frameFlicker;
        }
        bit |= mask;
    }

    std::array<std::uint8_t, W * H / 2> m_pixels{};
    std::array<std::uint8_t, W * H / 8> m_changed{};
    // The panel starts black.
    std::array<Color, Colours> m_colours{colors::Black};
    std::size_t m_colourCount = 1;
    std::uint32_t m_frameFlicker = 0;
    std::uint32_t m_flicker = 0;
    std::uint32_t m_worstFrame = 0;
};

} // namespace dmm::services
