// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tommi Tauriainen

#pragma once

#include <cstdint>

namespace dmm::services {

/// A fixed-cell 1-bit font covering code points first..last: each glyph is
/// height rows of ceil(width / 8) bytes, most significant bit leftmost.
struct Font
{
    std::uint8_t width;
    std::uint8_t height;
    std::uint8_t first;
    std::uint8_t last;
    const std::uint8_t *glyphs;

    constexpr unsigned bytesPerRow() const { return (width + 7u) / 8u; }

    /// Rows of the glyph for @p codePoint, '?' for one the font lacks.
    const std::uint8_t *glyph(std::uint32_t codePoint) const
    {
        if (codePoint < first || codePoint > last) {
            codePoint = '?';
        }
        return glyphs + (codePoint - first) * bytesPerRow() * height;
    }
};

namespace fonts {
// Spleen by Frederic Cambus (BSD-2-Clause). 5x8 is ASCII only; the others
// cover Latin-1 (up to U+00FF).
extern const Font Spleen5x8;
extern const Font Spleen6x12;
extern const Font Spleen8x16;
extern const Font Spleen12x24;
} // namespace fonts

} // namespace dmm::services
