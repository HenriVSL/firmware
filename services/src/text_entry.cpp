// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Henri Lavikainen

#include "dmm/services/text_entry.h"

#include <algorithm>

namespace dmm::services {

void TextEntry::begin(std::span<char> buffer, std::size_t length, const Options &options)
{
    m_buffer = buffer;
    m_length = std::min(length, buffer.size());
    m_cursor = m_length;
    m_options = options;
    m_case = Case::Upper;
    m_lastDigit = -1;
    m_tap = 0;
}

char TextEntry::shown(char c) const
{
    if (m_case == Case::Upper && c >= 'a' && c <= 'z') {
        return static_cast<char>(c - 'a' + 'A');
    }
    return c;
}

std::size_t TextEntry::letters(int digit, std::span<char> out) const
{
    if (digit < 0 || digit > 9 || m_options.letters == nullptr) {
        return 0;
    }
    std::size_t n = 0;
    for (const char c : (*m_options.letters)[static_cast<std::size_t>(digit)]) {
        const char s = shown(c);
        if (n < out.size() && (m_options.allowed == nullptr || m_options.allowed(s))) {
            out[n++] = s;
        }
    }
    return n;
}

void TextEntry::put(char c)
{
    if (m_options.overwrite) {
        if (m_cursor < m_buffer.size()) {
            m_buffer[m_cursor++] = c;
            m_length = std::max(m_length, m_cursor);
        }
        return;
    }
    if (m_length < m_buffer.size()) {
        std::copy_backward(m_buffer.begin() + static_cast<std::ptrdiff_t>(m_cursor),
            m_buffer.begin() + static_cast<std::ptrdiff_t>(m_length), m_buffer.begin() + static_cast<std::ptrdiff_t>(m_length) + 1);
        m_buffer[m_cursor++] = c;
        ++m_length;
    }
}

void TextEntry::digit(int digit, std::uint32_t nowMs)
{
    if (digit < 0 || digit > 9) {
        return;
    }
    if (m_case == Case::Digits) {
        m_lastDigit = -1;
        insert(static_cast<char>('0' + digit));
        return;
    }
    std::array<char, 16> set{};
    const std::size_t n = letters(digit, set);
    if (n == 0) {
        return;
    }
    const bool again = digit == m_lastDigit && nowMs - m_lastTap < m_options.tapMs && m_cursor > 0;
    m_tap = again ? (m_tap + 1) % n : 0;
    if (again) {
        m_buffer[m_cursor - 1] = set[m_tap];
    } else {
        put(set[m_tap]);
    }
    m_lastDigit = digit;
    m_lastTap = nowMs;
}

void TextEntry::backspace()
{
    m_lastDigit = -1;
    if (m_cursor == 0) {
        return;
    }
    std::copy(m_buffer.begin() + static_cast<std::ptrdiff_t>(m_cursor),
        m_buffer.begin() + static_cast<std::ptrdiff_t>(m_length), m_buffer.begin() + static_cast<std::ptrdiff_t>(m_cursor) - 1);
    --m_cursor;
    --m_length;
}

void TextEntry::clear()
{
    m_lastDigit = -1;
    m_length = 0;
    m_cursor = 0;
}

void TextEntry::nextCase()
{
    m_lastDigit = -1;
    switch (m_case) {
    case Case::Upper:
        m_case = m_options.lower ? Case::Lower : m_options.digits ? Case::Digits
                                                                  : Case::Upper;
        break;
    case Case::Lower:
        m_case = m_options.digits ? Case::Digits : Case::Upper;
        break;
    case Case::Digits:
        m_case = Case::Upper;
        break;
    }
}

void TextEntry::left()
{
    m_lastDigit = -1;
    if (m_cursor > 0) {
        --m_cursor;
    }
}

void TextEntry::right()
{
    m_lastDigit = -1;
    // In overwrite mode the cursor stays on the last place there is.
    if (m_cursor < m_length && (!m_options.overwrite || m_cursor + 1 < m_buffer.size())) {
        ++m_cursor;
    }
}

bool TextEntry::insert(char c)
{
    m_lastDigit = -1;
    if (m_options.allowed != nullptr && !m_options.allowed(c)) {
        return false;
    }
    const std::size_t before = m_length;
    const std::size_t at = m_cursor;
    put(c);
    return m_length != before || m_cursor != at;
}

void TextEntry::turn(int steps, std::string_view chars)
{
    m_lastDigit = -1;
    if (m_cursor >= m_buffer.size() || chars.empty()) {
        return;
    }
    if (m_cursor == m_length) {
        m_buffer[m_length++] = ' ';
    }
    const std::size_t at = chars.find(m_buffer[m_cursor]);
    const int n = static_cast<int>(chars.size());
    const int to = ((at == std::string_view::npos ? 0 : static_cast<int>(at)) + steps % n + n) % n;
    m_buffer[m_cursor] = chars[static_cast<std::size_t>(to)];
}

} // namespace dmm::services
