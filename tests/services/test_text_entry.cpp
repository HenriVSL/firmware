// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Henri Lavikainen

#include "dmm/services/text_entry.h"

#include <doctest/doctest.h>

#include <array>
#include <string>

using dmm::services::TextEntry;

namespace {

constexpr TextEntry::KeyLetters Letters = {" 0", ".,?-/1", "abc2", "def3", "ghi4", "jkl5", "mno6", "pqrs7", "tuv8", "wxyz9"};

struct Typing
{
    std::array<char, 12> buffer{};
    TextEntry entry;
    std::uint32_t now = 1000;

    explicit Typing(TextEntry::Options options = {&Letters})
    {
        if (options.letters == nullptr) {
            options.letters = &Letters;
        }
        entry.begin(buffer, 0, options);
    }
    // Taps 300 ms apart; a space in @p keys is a pause.
    void tap(std::string_view keys)
    {
        for (const char k : keys) {
            now += k == ' ' ? 2000 : 300;
            if (k != ' ') {
                entry.digit(k - '0', now);
            }
        }
    }
    std::string text() const { return std::string(entry.text()); }
};

bool noLowerOrComma(char c)
{
    return !(c >= 'a' && c <= 'z') && c != ',';
}

} // namespace

TEST_CASE("text entry: taps step through a key's letters, another key or a pause keeps one")
{
    Typing t;
    t.tap("44 444");
    CHECK(t.text() == "HI");
    t.tap("2");
    t.tap(" 22");
    CHECK(t.text() == "HIAB");
    CHECK(t.entry.lastDigit() == 2);
    CHECK(t.entry.tapIndex() == 1);
    // Round the key's letters: a b c 2 a.
    t.tap(" 22222");
    CHECK(t.text() == "HIABA");
    t.tap(" 0");
    CHECK(t.text() == "HIABA ");
}

TEST_CASE("text entry: case and digits")
{
    Typing t;
    t.tap("2");
    t.entry.nextCase();
    CHECK(t.entry.textCase() == TextEntry::Case::Lower);
    t.tap("2");
    t.entry.nextCase();
    CHECK(t.entry.textCase() == TextEntry::Case::Digits);
    t.tap("222");
    CHECK(t.text() == "Aa222");
    t.entry.nextCase();
    CHECK(t.entry.textCase() == TextEntry::Case::Upper);
    char out[8];
    REQUIRE(t.entry.letters(7, out) == 5);
    CHECK(std::string(out, 5) == "PQRS7");
}

TEST_CASE("text entry: a filter leaves out what the mode cannot send")
{
    Typing t({&Letters, 1200, false, false, true, noLowerOrComma});
    t.entry.nextCase();
    CHECK(t.entry.textCase() == TextEntry::Case::Digits);
    t.entry.nextCase();
    t.tap("11");
    CHECK(t.text() == "?");
    CHECK_FALSE(t.entry.insert('a'));
    CHECK(t.entry.insert('Z'));
    CHECK(t.text() == "?Z");
}

TEST_CASE("text entry: the cursor, deleting and typing in the middle")
{
    Typing t;
    t.tap("2 3 4");
    CHECK(t.text() == "ADG");
    t.entry.left();
    t.entry.left();
    t.tap("5");
    CHECK(t.text() == "AJDG");
    CHECK(t.entry.cursor() == 2);
    t.entry.backspace();
    CHECK(t.text() == "ADG");
    t.entry.right();
    t.entry.right();
    t.entry.right();
    CHECK(t.entry.cursor() == 3);
    t.entry.clear();
    CHECK(t.text().empty());
    t.entry.backspace();
    CHECK(t.entry.cursor() == 0);
}

TEST_CASE("text entry: overwriting, and a full buffer")
{
    Typing t({&Letters, 1200, true, true, false, nullptr});
    t.tap("2 3 4");
    t.entry.left();
    t.entry.left();
    t.tap("5");
    CHECK(t.text() == "AJG");
    for (int i = 0; i < 20; ++i) {
        t.tap(" 8");
    }
    CHECK(t.text().size() == 12);
    CHECK(t.entry.cursor() == 12);
    t.entry.left();
    t.entry.right();
    t.entry.right();
    CHECK(t.entry.cursor() == 11);
    // Case goes upper and lower only.
    t.entry.nextCase();
    t.entry.nextCase();
    CHECK(t.entry.textCase() == TextEntry::Case::Upper);
}

TEST_CASE("text entry: the knob steps the character at the cursor")
{
    Typing t({&Letters, 1200, true, true, false, nullptr});
    t.entry.turn(2, " ABC");
    CHECK(t.text() == "B");
    t.entry.turn(-3, " ABC");
    CHECK(t.text() == "C");
    t.entry.right();
    CHECK(t.entry.cursor() == 1);
    t.entry.turn(1, " ABC");
    CHECK(t.text() == "CA");
}
