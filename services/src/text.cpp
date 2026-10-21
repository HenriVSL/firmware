// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tommi Tauriainen

#include "dmm/services/display.h"

namespace dmm::services {

namespace {

// Decodes one UTF-8 character starting at @p i and advances past it.
// Malformed input yields U+FFFD, which every font shows as '?'.
std::uint32_t nextCodePoint(std::string_view text, std::size_t &i)
{
    const auto lead = static_cast<std::uint8_t>(text[i++]);
    if (lead < 0x80) {
        return lead;
    }
    unsigned extra = 0;
    std::uint32_t cp = 0;
    if ((lead & 0xE0) == 0xC0) {
        extra = 1;
        cp = lead & 0x1Fu;
    } else if ((lead & 0xF0) == 0xE0) {
        extra = 2;
        cp = lead & 0x0Fu;
    } else if ((lead & 0xF8) == 0xF0) {
        extra = 3;
        cp = lead & 0x07u;
    } else {
        return 0xFFFD;
    }
    for (unsigned k = 0; k < extra; ++k) {
        if (i >= text.size() || (static_cast<std::uint8_t>(text[i]) & 0xC0) != 0x80) {
            return 0xFFFD;
        }
        cp = (cp << 6) | (static_cast<std::uint8_t>(text[i++]) & 0x3Fu);
    }
    return cp;
}

} // namespace

int drawText(Display &display, const Font &font, int x, int y, std::string_view text, Color fg, Color bg)
{
    std::size_t i = 0;
    while (i < text.size() && x < display.width()) {
        const std::uint32_t cp = nextCodePoint(text, i);
        display.drawBitmap(x, y, font.width, font.height, font.glyph(cp), static_cast<int>(font.bytesPerRow()), fg, bg);
        x += font.width;
    }
    return x;
}

} // namespace dmm::services
