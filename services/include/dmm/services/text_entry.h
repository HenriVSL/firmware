// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Henri Lavikainen

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace dmm::services {

/// Text typed on the digit keys as on old keypad phones: pressing a key
/// again soon after steps through its characters in the same place;
/// another key, or a pause, keeps the character shown. The caller owns the
/// buffer and draws. Heap-free.
class TextEntry
{
public:
    /// The characters on each digit key, 0 to 9, in lower case.
    using KeyLetters = std::array<std::string_view, 10>;

    enum class Case : std::uint8_t
    {
        Upper,
        Lower,
        Digits, // a digit key types its digit at once
    };

    struct Options
    {
        const KeyLetters *letters = nullptr;
        /// A press after this long starts a new character.
        std::uint32_t tapMs = 1200;
        /// Typing replaces the character at the cursor rather than going in
        /// before it.
        bool overwrite = false;
        /// Lower case, and digits typed at once, may be chosen.
        bool lower = true;
        bool digits = true;
        /// Whether @p c may be typed; nullptr: all.
        bool (*allowed)(char c) = nullptr;
    };

    /// Starts on @p buffer holding @p length characters, the cursor at the
    /// end, in upper case.
    void begin(std::span<char> buffer, std::size_t length, const Options &options);

    /// Digit key @p digit (0..9) pressed at @p nowMs.
    void digit(int digit, std::uint32_t nowMs);
    /// The next press starts a new character.
    void accept() { m_lastDigit = -1; }
    /// Deletes the character before the cursor.
    void backspace();
    void clear();
    /// Upper, lower, digits (each when allowed), and round again.
    void nextCase();
    void setCase(Case c) { m_case = c; }
    void left();
    void right();
    /// Puts @p c in at the cursor; false if full or not allowed.
    bool insert(char c);
    /// Steps the character at the cursor @p steps places through @p chars
    /// (a knob); at the end a space comes first.
    void turn(int steps, std::string_view chars);

    std::string_view text() const { return {m_buffer.data(), m_length}; }
    std::size_t length() const { return m_length; }
    std::size_t cursor() const { return m_cursor; }
    std::size_t capacity() const { return m_buffer.size(); }
    Case textCase() const { return m_case; }
    /// The key last tapped (-1 after any other key), and which of its
    /// characters the taps have reached.
    int lastDigit() const { return m_lastDigit; }
    std::size_t tapIndex() const { return m_tap; }
    /// The characters @p digit types now, case and filter applied, into
    /// @p out; their number.
    std::size_t letters(int digit, std::span<char> out) const;

private:
    char shown(char c) const;
    void put(char c);

    std::span<char> m_buffer;
    std::size_t m_length = 0;
    std::size_t m_cursor = 0;
    Options m_options;
    Case m_case = Case::Upper;
    int m_lastDigit = -1;
    std::uint32_t m_lastTap = 0;
    std::size_t m_tap = 0;
};

} // namespace dmm::services
