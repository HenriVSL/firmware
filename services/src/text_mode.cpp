// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Henri Lavikainen

#include "dmm/services/text_mode.h"

#include <algorithm>

namespace dmm::services::text_mode {

void TextLines::clear()
{
    m_length = {};
    m_newest = 0;
    m_older = 0;
    m_afterCr = false;
    ++m_version;
}

void TextLines::newLine()
{
    m_newest = (m_newest + 1) % Lines;
    m_length[m_newest] = 0;
    m_older = std::min(m_older + 1, Lines - 1);
}

void TextLines::add(char c)
{
    const bool afterCr = m_afterCr;
    m_afterCr = c == '\r';
    if (c == '\r' || c == '\n') {
        // CR LF is one new line.
        if (!(c == '\n' && afterCr)) {
            newLine();
            ++m_version;
        }
        return;
    }
    if (c < ' ' || c > '~') {
        return;
    }
    std::uint8_t &length = m_length[m_newest];
    if (length == Width) {
        if (c == ' ') {
            newLine();
            ++m_version;
            return;
        }
        // The word being cut goes to the next line whole, if it is not
        // the whole line.
        std::array<char, Width> &full = m_text[m_newest];
        std::size_t space = Width;
        while (space > 0 && full[space - 1] != ' ') {
            --space;
        }
        const std::size_t from = m_newest;
        newLine();
        if (space > 0) {
            const std::size_t moved = Width - space;
            std::copy_n(m_text[from].begin() + static_cast<std::ptrdiff_t>(space), moved, m_text[m_newest].begin());
            m_length[m_newest] = static_cast<std::uint8_t>(moved);
            m_length[from] = static_cast<std::uint8_t>(space - 1);
        }
    }
    m_text[m_newest][m_length[m_newest]++] = c;
    ++m_version;
}

std::string_view TextLines::line(std::size_t back) const
{
    if (back > m_older) {
        return {};
    }
    const std::size_t i = (m_newest + Lines - back) % Lines;
    return {m_text[i].data(), m_length[i]};
}

lib::StaticString<12> frequencyText(std::uint32_t hz)
{
    lib::StaticString<12> s;
    s << hz / 1'000'000 << '.' << lib::zeroPadded(hz / 1000 % 1000, 3);
    if (hz % 1000 != 0) {
        s << (hz / 100 % 10);
    }
    return s;
}

HomeRows homeRows(const HomeInput &in)
{
    HomeRows r;
    r.top << "RTTY " << frequencyText(in.frequencyHz).view();
    if (!in.call.empty()) {
        r.big << in.call;
    } else {
        r.big << (in.signal ? "Receiving" : "Listening");
    }
    r.older << in.older;
    r.newest << in.newest;
    const std::size_t slash = in.tones.find('/');
    if (in.reverse && slash != std::string_view::npos) {
        r.tones << std::string_view(in.tones.data() + slash + 1, in.tones.size() - slash - 1) << '/'
                << std::string_view(in.tones.data(), slash);
    } else {
        r.tones << in.tones;
    }
    const int percent = static_cast<int>(std::clamp(in.quality, 0.0F, 1.0F) * 100.0F + 0.5F);
    r.quality << percent << '%';
    r.qualityWarn = !in.decoding;
    r.bottom << in.baud << " Bd  " << in.characters << " chr  " << in.errors << " err";
    return r;
}

} // namespace dmm::services::text_mode
