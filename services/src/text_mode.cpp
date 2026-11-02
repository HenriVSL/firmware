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

namespace {

constexpr Macro Macros[MacroCount] = {
    {"CQ", "CQ CQ CQ DE $MYCALL $MYCALL $MYCALL PSE K"},
    {"Answer", "$CALL DE $MYCALL $MYCALL K"},
    {"QTH", "$CALL DE $MYCALL QTH $LOC $LOC KN"},
    {"Over", "$CALL DE $MYCALL KN"},
    {"73", "$CALL DE $MYCALL 73 SK"},
    {"Test", "RYRYRYRYRYRY THE QUICK BROWN FOX JUMPS OVER THE LAZY DOG 0123456789 DE $MYCALL"},
};

char upper(char c)
{
    return c >= 'a' && c <= 'z' ? static_cast<char>(c - 'a' + 'A') : c;
}

bool wordCharacter(char c)
{
    c = upper(c);
    return (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '/';
}

} // namespace

const Macro &macro(std::size_t index)
{
    return Macros[index < MacroCount ? index : 0];
}

FillResult fill(std::string_view text, const FillValues &values, std::span<char> out, std::size_t &length)
{
    struct Placeholder
    {
        std::string_view name;
        std::string_view value;
        FillResult missing;
    };
    // $MYCALL before $CALL's shorter name could match inside it.
    const Placeholder placeholders[] = {
        {"$MYCALL", values.myCall, FillResult::NoCall},
        {"$CALL", values.theirCall, FillResult::NoTheirCall},
        {"$LOC", values.locator, FillResult::NoLocator},
        {"$UTC", values.utc, FillResult::Done},
    };
    length = 0;
    auto put = [&](std::string_view part) {
        if (length + part.size() > out.size()) {
            return false;
        }
        for (const char c : part) {
            out[length++] = c;
        }
        return true;
    };
    std::size_t i = 0;
    while (i < text.size()) {
        bool replaced = false;
        if (text[i] == '$') {
            for (const Placeholder &p : placeholders) {
                const std::size_t n = std::min(p.name.size(), text.size() - i);
                if (std::string_view(text.data() + i, n) == p.name) {
                    if (p.value.empty() && p.missing != FillResult::Done) {
                        return p.missing;
                    }
                    if (!put(p.value)) {
                        return FillResult::TooLong;
                    }
                    i += p.name.size();
                    replaced = true;
                    break;
                }
            }
        }
        if (!replaced) {
            if (!put(std::string_view(text.data() + i, 1))) {
                return FillResult::TooLong;
            }
            ++i;
        }
    }
    return FillResult::Done;
}

bool namesCall(std::string_view text, std::string_view call)
{
    if (call.empty() || call.size() > text.size()) {
        return false;
    }
    for (std::size_t i = 0; i + call.size() <= text.size(); ++i) {
        if (i > 0 && wordCharacter(text[i - 1])) {
            continue;
        }
        const std::size_t end = i + call.size();
        if (end < text.size() && wordCharacter(text[end])) {
            continue;
        }
        bool same = true;
        for (std::size_t k = 0; k < call.size() && same; ++k) {
            same = upper(text[i + k]) == upper(call[k]);
        }
        if (same) {
            return true;
        }
    }
    return false;
}

lib::StaticString<8> locator(std::int32_t latitudeE7, std::int32_t longitudeE7)
{
    // In units of 1/24 of the subsquare's size: lon 5', lat 2.5'.
    constexpr std::int64_t E7 = 10'000'000;
    std::int64_t lon = static_cast<std::int64_t>(longitudeE7) + 180 * E7;
    std::int64_t lat = static_cast<std::int64_t>(latitudeE7) + 90 * E7;
    lon = std::clamp<std::int64_t>(lon, 0, 360 * E7 - 1);
    lat = std::clamp<std::int64_t>(lat, 0, 180 * E7 - 1);
    // Subsquares: 1/12 degree of longitude, 1/24 of latitude.
    const std::int64_t lonSub = lon * 12 / E7;
    const std::int64_t latSub = lat * 24 / E7;
    lib::StaticString<8> s;
    s << static_cast<char>('A' + lonSub / (20 * 12)) << static_cast<char>('A' + latSub / (10 * 24))
      << static_cast<char>('0' + lonSub / 24 % 10) << static_cast<char>('0' + latSub / 24 % 10)
      << static_cast<char>('A' + lonSub % 24) << static_cast<char>('A' + latSub % 24);
    return s;
}

const char *sendingText(Sending s)
{
    switch (s) {
    case Sending::Waiting:
        return "Waiting for the channel";
    case Sending::OnAir:
        return "Sending";
    case Sending::Sent:
        return "Sent";
    case Sending::Busy:
        return "Channel busy";
    case Sending::Stopped:
        return "Sending stopped";
    case Sending::Refused:
        return "Not sent";
    case Sending::TooLong:
        return "Text too long";
    case Sending::NoCall:
        return "No callsign";
    case Sending::NoTheirCall:
        return "No call heard";
    case Sending::NoPosition:
        return "No position";
    case Sending::Nothing:
        return "Nothing to send";
    case Sending::Idle:
        break;
    }
    return "";
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
    switch (in.sending) {
    case Sending::Idle:
        if (in.composing > 0) {
            r.bottom << "Composing " << in.composing;
        } else {
            r.bottom << in.baud << " Bd  " << in.characters << " chr  " << in.errors << " err";
        }
        break;
    case Sending::OnAir:
        r.bottom << (in.dry ? "Loopback " : "Sending ") << in.sendingS << " s";
        break;
    default:
        r.bottom << sendingText(in.sending);
        r.bottomWarn = in.sending != Sending::Waiting && in.sending != Sending::Sent;
        break;
    }
    return r;
}

} // namespace dmm::services::text_mode
