// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tommi Tauriainen

#include "dmm/services/display.h"

#include <doctest/doctest.h>

#include <vector>

using namespace dmm::services;

namespace {

class RecordingDisplay final : public Display
{
public:
    void start() override {}
    int width() const override { return 160; }
    int height() const override { return 128; }
    void fillRect(int, int, int, int, Color) override {}
    void drawBitmap(int x, int, int, int, const std::uint8_t *rows, int, Color, Color) override
    {
        xs.push_back(x);
        glyphs.push_back(rows);
    }
    void setBacklight(int) override {}
    void setPanelOn(bool) override {}

    std::vector<int> xs;
    std::vector<const std::uint8_t *> glyphs;
};

} // namespace

TEST_CASE("text advances by the cell width and decodes UTF-8 to Latin-1 glyphs")
{
    RecordingDisplay d;
    const Font &f = fonts::Spleen6x12;
    CHECK(drawText(d, f, 4, 0, "Aä€", colors::White, colors::Black) == 4 + 3 * 6);
    CHECK(d.xs == std::vector<int>{4, 10, 16});
    CHECK(d.glyphs[0] == f.glyph('A'));
    CHECK(d.glyphs[1] == f.glyph(0xE4));
    CHECK(d.glyphs[2] == f.glyph('?')); // the euro sign is not in Latin-1
}

TEST_CASE("broken UTF-8 shows as '?' and drawing stops at the right edge")
{
    RecordingDisplay d;
    const Font &f = fonts::Spleen12x24;
    drawText(d, f, 0, 0, "\xC3", colors::White, colors::Black);
    CHECK(d.glyphs.back() == f.glyph('?'));
    d.xs.clear();
    drawText(d, f, 0, 0, "0123456789ABCDEF", colors::White, colors::Black);
    CHECK(d.xs.size() == 14); // 14 * 12 = 168 > 160: the 14th starts at 156
}

TEST_CASE("fonts have the expected cells and Latin-1 coverage")
{
    CHECK(fonts::Spleen5x8.width == 5);
    CHECK(fonts::Spleen5x8.last == 126);
    CHECK(fonts::Spleen6x12.last == 255);
    CHECK(fonts::Spleen8x16.height == 16);
    CHECK(fonts::Spleen12x24.bytesPerRow() == 2);
    // 'ä' differs from 'a' and from '?'.
    const auto rows = [](const Font &f, std::uint32_t cp) {
        return std::vector<std::uint8_t>(f.glyph(cp), f.glyph(cp) + f.bytesPerRow() * f.height);
    };
    CHECK(rows(fonts::Spleen6x12, 0xE4) != rows(fonts::Spleen6x12, 'a'));
    CHECK(rows(fonts::Spleen6x12, 0xE4) != rows(fonts::Spleen6x12, '?'));
}
