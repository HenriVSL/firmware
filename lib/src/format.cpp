// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Henri Lavikainen

#include "dmm/lib/format.h"

namespace dmm::lib::detail {

namespace {

template <typename T>
void write(std::span<char> buffer, TextEnd &end, const T &value)
{
    TextWriter w(buffer.subspan(end.length));
    w << value;
    end.length += w.size();
    end.truncated = end.truncated || w.truncated();
}

} // namespace

void append(std::span<char> buffer, TextEnd &end, std::string_view s)
{
    write(buffer, end, s);
}

void append(std::span<char> buffer, TextEnd &end, const char *s)
{
    write(buffer, end, std::string_view(s));
}

void append(std::span<char> buffer, TextEnd &end, char c)
{
    write(buffer, end, c);
}

void append(std::span<char> buffer, TextEnd &end, bool b)
{
    write(buffer, end, b);
}

void append(std::span<char> buffer, TextEnd &end, Hex h)
{
    write(buffer, end, h);
}

void append(std::span<char> buffer, TextEnd &end, ZeroPadded z)
{
    write(buffer, end, z);
}

void append(std::span<char> buffer, TextEnd &end, std::int32_t value)
{
    write(buffer, end, value);
}

void append(std::span<char> buffer, TextEnd &end, std::uint32_t value)
{
    write(buffer, end, value);
}

void append(std::span<char> buffer, TextEnd &end, std::int64_t value)
{
    write(buffer, end, value);
}

void append(std::span<char> buffer, TextEnd &end, std::uint64_t value)
{
    write(buffer, end, value);
}

} // namespace dmm::lib::detail
