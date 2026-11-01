// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Henri Lavikainen

#include "dmm/services/text_mode.h"

#include <doctest/doctest.h>

#include <string>
#include <string_view>

namespace text_mode = dmm::services::text_mode;
using text_mode::TextLines;

namespace {

void add(TextLines &t, std::string_view s)
{
    for (const char c : s) {
        t.add(c);
    }
}

} // namespace

TEST_CASE("text lines: CR, LF and CR LF each start one line")
{
    TextLines t;
    CHECK(t.count() == 1);
    CHECK(t.line(0).empty());
    add(t, "ONE\r\nTWO\nTHREE\rFOUR");
    CHECK(t.count() == 4);
    CHECK(t.line(0) == "FOUR");
    CHECK(t.line(1) == "THREE");
    CHECK(t.line(2) == "TWO");
    CHECK(t.line(3) == "ONE");
    CHECK(t.line(4).empty());
}

TEST_CASE("text lines: a long line wraps at the last space, or hard without one")
{
    TextLines t;
    add(t, "THE QUICK BROWN FOX JUMPS OVER THE LAZY DOG");
    CHECK(t.line(1) == "THE QUICK BROWN FOX JUMPS OVER");
    CHECK(t.line(0) == "THE LAZY DOG");
    t.clear();
    add(t, std::string(40, 'R'));
    CHECK(t.line(1) == std::string(TextLines::Width, 'R'));
    CHECK(t.line(0) == std::string(40 - TextLines::Width, 'R'));
    t.clear();
    add(t, std::string(TextLines::Width, 'Y') + " Z");
    CHECK(t.line(1) == std::string(TextLines::Width, 'Y'));
    CHECK(t.line(0) == "Z");
}

TEST_CASE("text lines: the oldest lines go, and control characters are left out")
{
    TextLines t;
    for (std::size_t i = 0; i < TextLines::Lines + 5; ++i) {
        add(t, "L" + std::to_string(i) + "\a\n");
    }
    CHECK(t.count() == TextLines::Lines);
    CHECK(t.line(0).empty());
    CHECK(t.line(1) == "L" + std::to_string(TextLines::Lines + 4));
    CHECK(t.line(TextLines::Lines - 1) == "L6");
    const std::uint32_t v = t.version();
    t.add('\a');
    CHECK(t.version() == v);
    t.add('X');
    CHECK(t.version() != v);
}

TEST_CASE("the text mode home screen")
{
    text_mode::HomeInput in;
    in.frequencyHz = 144'612'500;
    in.older = "CQ CQ DE N0CALL";
    in.newest = "PSE K";
    in.decoding = true;
    in.signal = true;
    in.quality = 0.724F;
    in.tones = "2125/2295";
    in.baud = "45.45";
    in.characters = 123;
    in.errors = 4;
    text_mode::HomeRows r = text_mode::homeRows(in);
    CHECK(r.top.view() == "RTTY 144.6125");
    CHECK(r.big.view() == "Receiving");
    CHECK(r.older.view() == "CQ CQ DE N0CALL");
    CHECK(r.newest.view() == "PSE K");
    CHECK(r.tones.view() == "2125/2295");
    CHECK(r.quality.view() == "72%");
    CHECK_FALSE(r.qualityWarn);
    CHECK(r.bottom.view() == "45.45 Bd  123 chr  4 err");

    in.call = "N0CALL";
    in.reverse = true;
    in.decoding = false;
    in.frequencyHz = 433'625'000;
    r = text_mode::homeRows(in);
    CHECK(r.top.view() == "RTTY 433.625");
    CHECK(r.big.view() == "N0CALL");
    CHECK(r.tones.view() == "2295/2125");
    CHECK(r.qualityWarn);

    in.call = {};
    in.signal = false;
    CHECK(text_mode::homeRows(in).big.view() == "Listening");
}
