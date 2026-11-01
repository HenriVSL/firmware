// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tommi Tauriainen

#pragma once

#include <charconv>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <type_traits>

namespace dmm::lib {

/// Hexadecimal formatting request for TextWriter, zero-padded to @c digits.
struct Hex
{
    std::uint32_t value;
    std::uint8_t digits;
};

constexpr Hex hex(std::uint32_t value, std::uint8_t digits = 8)
{
    return {value, digits};
}

/// Decimal formatting request for TextWriter, zero-padded to at least
/// @c digits.
struct ZeroPadded
{
    std::uint32_t value;
    std::uint8_t digits;
};

constexpr ZeroPadded zeroPadded(std::uint32_t value, std::uint8_t digits)
{
    return {value, digits};
}

/// Appends text and numbers to a caller-provided buffer, without printf
/// and without the heap. Output past the buffer is dropped and remembered.
class TextWriter
{
public:
    explicit TextWriter(std::span<char> buffer)
        : m_buf(buffer)
    {
    }

    TextWriter &operator<<(std::string_view s)
    {
        for (char c : s) {
            put(c);
        }
        return *this;
    }

    TextWriter &operator<<(const char *s) { return *this << std::string_view(s); }

    TextWriter &operator<<(char c)
    {
        put(c);
        return *this;
    }

    TextWriter &operator<<(bool b) { return *this << (b ? std::string_view("true") : std::string_view("false")); }

    template <std::integral I>
        requires(!std::same_as<I, char> && !std::same_as<I, bool>)
    TextWriter &operator<<(I value)
    {
        char tmp[24];
        auto res = std::to_chars(tmp, tmp + sizeof(tmp), value);
        return *this << std::string_view(tmp, static_cast<std::size_t>(res.ptr - tmp));
    }

    TextWriter &operator<<(Hex h)
    {
        static constexpr char digits[] = "0123456789abcdef";
        for (int i = h.digits - 1; i >= 0; --i) {
            put(digits[(h.value >> (4 * i)) & 0xFu]);
        }
        return *this;
    }

    TextWriter &operator<<(ZeroPadded z)
    {
        char tmp[12];
        const auto res = std::to_chars(tmp, tmp + sizeof(tmp), z.value);
        const auto length = static_cast<std::size_t>(res.ptr - tmp);
        for (std::size_t i = length; i < z.digits; ++i) {
            put('0');
        }
        return *this << std::string_view(tmp, length);
    }

    std::string_view view() const { return {m_buf.data(), m_len}; }
    std::size_t size() const { return m_len; }
    bool truncated() const { return m_truncated; }

private:
    void put(char c)
    {
        if (m_len < m_buf.size()) {
            m_buf[m_len++] = c;
        } else {
            m_truncated = true;
        }
    }

    std::span<char> m_buf;
    std::size_t m_len = 0;
    bool m_truncated = false;
};

namespace detail {

/// How much of a StaticString is filled, and whether text was dropped.
struct TextEnd
{
    std::size_t length = 0;
    bool truncated = false;
};

// StaticString's appends, built once for every capacity: each writes
// through a TextWriter after end.length and moves the end.
void append(std::span<char> buffer, TextEnd &end, std::string_view s);
void append(std::span<char> buffer, TextEnd &end, const char *s);
void append(std::span<char> buffer, TextEnd &end, char c);
void append(std::span<char> buffer, TextEnd &end, bool b);
void append(std::span<char> buffer, TextEnd &end, Hex h);
void append(std::span<char> buffer, TextEnd &end, ZeroPadded z);
void append(std::span<char> buffer, TextEnd &end, std::int32_t value);
void append(std::span<char> buffer, TextEnd &end, std::uint32_t value);
void append(std::span<char> buffer, TextEnd &end, std::int64_t value);
void append(std::span<char> buffer, TextEnd &end, std::uint64_t value);

template <std::integral I>
    requires(!std::same_as<I, char> && !std::same_as<I, bool>)
void append(std::span<char> buffer, TextEnd &end, I value)
{
    if constexpr (std::is_signed_v<I>) {
        append(buffer, end, static_cast<std::conditional_t<sizeof(I) <= 4, std::int32_t, std::int64_t>>(value));
    } else {
        append(buffer, end, static_cast<std::conditional_t<sizeof(I) <= 4, std::uint32_t, std::uint64_t>>(value));
    }
}

} // namespace detail

/// Fixed-capacity string that accepts everything TextWriter does.
template <std::size_t N>
class StaticString
{
public:
    StaticString() = default;
    StaticString(std::string_view s) { *this << s; } // NOLINT(google-explicit-constructor)

    template <typename T>
    StaticString &operator<<(const T &value)
    {
        detail::append(m_data, m_end, value);
        return *this;
    }

    void clear() { m_end = {}; }

    std::string_view view() const { return {m_data, m_end.length}; }
    std::size_t size() const { return m_end.length; }
    static constexpr std::size_t capacity() { return N; }
    bool truncated() const { return m_end.truncated; }

private:
    char m_data[N]{};
    detail::TextEnd m_end;
};

} // namespace dmm::lib
