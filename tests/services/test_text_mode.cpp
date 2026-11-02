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

TEST_CASE("the text mode home screen while sending")
{
    text_mode::HomeInput in;
    in.baud = "45.45";
    in.composing = 23;
    CHECK(text_mode::homeRows(in).bottom.view() == "Composing 23");
    in.sending = text_mode::Sending::Waiting;
    CHECK(text_mode::homeRows(in).bottom.view() == "Waiting for the channel");
    CHECK_FALSE(text_mode::homeRows(in).bottomWarn);
    in.sending = text_mode::Sending::OnAir;
    in.sendingS = 12;
    CHECK(text_mode::homeRows(in).bottom.view() == "Sending 12 s");
    in.dry = true;
    CHECK(text_mode::homeRows(in).bottom.view() == "Loopback 12 s");
    in.sending = text_mode::Sending::Busy;
    CHECK(text_mode::homeRows(in).bottom.view() == "Channel busy");
    CHECK(text_mode::homeRows(in).bottomWarn);
}

TEST_CASE("macros: placeholders filled in, or what is missing")
{
    char out[text_mode::MaxSendChars];
    std::size_t n = 0;
    const text_mode::FillValues v{"N0CALL", "Q0TEST", "KP20LE", "12:34"};
    REQUIRE(text_mode::fill("$CALL DE $MYCALL QTH $LOC AT $UTC", v, out, n) == text_mode::FillResult::Done);
    CHECK(std::string_view(out, n) == "Q0TEST DE N0CALL QTH KP20LE AT 12:34");
    CHECK(text_mode::fill(text_mode::macro(0).text, {"N0CALL", {}, {}, {}}, out, n) == text_mode::FillResult::Done);
    CHECK(std::string_view(out, n) == "CQ CQ CQ DE N0CALL N0CALL N0CALL PSE K");
    CHECK(text_mode::fill(text_mode::macro(1).text, {"N0CALL", {}, {}, {}}, out, n) == text_mode::FillResult::NoTheirCall);
    CHECK(text_mode::fill(text_mode::macro(0).text, {}, out, n) == text_mode::FillResult::NoCall);
    CHECK(text_mode::fill(text_mode::macro(2).text, {"N0CALL", "Q0TEST", {}, {}}, out, n) == text_mode::FillResult::NoLocator);
    char small[10];
    CHECK(text_mode::fill("$MYCALL $MYCALL", v, small, n) == text_mode::FillResult::TooLong);
    CHECK(text_mode::fill("50$ $X", v, out, n) == text_mode::FillResult::Done);
    CHECK(std::string_view(out, n) == "50$ $X");
    for (std::size_t i = 0; i < text_mode::MacroCount; ++i) {
        CHECK(std::string_view(text_mode::macro(i).name).size() > 0);
        CHECK(text_mode::fill(text_mode::macro(i).text, v, out, n) == text_mode::FillResult::Done);
    }
}

TEST_CASE("a callsign as a word of its own")
{
    CHECK(text_mode::namesCall("CQ DE N0CALL K", "N0CALL"));
    CHECK(text_mode::namesCall("de n0call", "N0CALL"));
    CHECK(text_mode::namesCall("N0CALL", "N0CALL"));
    CHECK(text_mode::namesCall("N0CALL, K", "N0CALL"));
    CHECK_FALSE(text_mode::namesCall("N0CALLS K", "N0CALL"));
    CHECK_FALSE(text_mode::namesCall("XN0CALL", "N0CALL"));
    CHECK_FALSE(text_mode::namesCall("N0CALL/P", "N0CALL"));
    CHECK_FALSE(text_mode::namesCall("HELLO", ""));
}

TEST_CASE("the Maidenhead locator of a position")
{
    // Made-up places: 60.17 N 24.94 E and 33.86 S 151.21 E.
    CHECK(text_mode::locator(601'700'000, 249'400'000).view() == "KP20LE");
    CHECK(text_mode::locator(-338'600'000, 1'512'100'000).view() == "QF56OD");
    CHECK(text_mode::locator(0, 0).view() == "JJ00AA");
    CHECK(text_mode::locator(900'000'000, 1'800'000'000).view() == "RR99XX");
}
