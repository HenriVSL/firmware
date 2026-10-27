// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tommi Tauriainen

#include "standby.h"

#include "dmm/lib/format.h"
#include "dmm/services/font.h"

#include <iterator>

namespace dmm::app::ui {

namespace {

using namespace services;
namespace p = palette;

const Font &Small = fonts::Spleen5x8;
const Font &Body = fonts::Spleen6x12;
const Font &Medium = fonts::Spleen8x16;
const Font &Large = fonts::Spleen12x24;

int textWidth(const Font &f, std::string_view s)
{
    return static_cast<int>(s.size()) * f.width;
}

// A filled tag such as "TX" in the status bar; returns the x after it.
int tag(Display &d, int x, std::string_view text, Color fill, Color ink)
{
    const int w = textWidth(Small, text) + 4;
    d.fillRect(x, 1, w, StandbyScreen::StatusH - 2, fill);
    drawText(d, Small, x + 2, 2, text, ink, fill);
    return x + w + 2;
}

// Frequency as MHz with four decimals and no trailing zero past three.
lib::StaticString<12> mhz(std::uint32_t hz)
{
    lib::StaticString<12> s;
    const std::uint32_t tenthKhz = hz / 100;
    s << tenthKhz / 10000 << '.';
    const std::uint32_t frac = tenthKhz % 10000;
    s << static_cast<char>('0' + frac / 1000) << static_cast<char>('0' + frac / 100 % 10)
      << static_cast<char>('0' + frac / 10 % 10);
    if (frac % 10 != 0) {
        s << static_cast<char>('0' + frac % 10);
    }
    return s;
}

lib::StaticString<12> toneText(const Tone &t)
{
    lib::StaticString<12> s;
    if (t.kind == Tone::Kind::Ctcss) {
        s << t.value / 10 << '.' << static_cast<char>('0' + t.value % 10);
    } else if (t.kind == Tone::Kind::Dcs) {
        s << 'D' << static_cast<char>('0' + (t.value >> 6 & 7)) << static_cast<char>('0' + (t.value >> 3 & 7))
          << static_cast<char>('0' + (t.value & 7)) << (t.inverted ? 'I' : 'N');
    }
    return s;
}

const char *powerText(PowerLevel p)
{
    constexpr const char *Names[] = {"LOW", "MID", "HIGH", "TURBO"};
    return Names[static_cast<int>(p) & 3];
}

// The typed digits as a frequency with the rest of its places shown as
// underscores: "1453" -> "145.3__".
lib::StaticString<12> entryText(std::string_view digits)
{
    lib::StaticString<12> s;
    for (std::size_t i = 0; i < 6; ++i) {
        if (i == 3) {
            s << '.';
        }
        s << (i < digits.size() ? digits[i] : '_');
    }
    return s;
}

// Copies @p text into a fixed buffer, cut to fit.
template <std::size_t N>
void copyText(char (&out)[N], std::string_view text)
{
    std::size_t n = 0;
    for (; n < text.size() && n + 1 < N; ++n) {
        out[n] = text[n];
    }
    out[n] = '\0';
}

void line(Display &d, const Font &f, int x, int y, int w, std::string_view text, Color ink)
{
    const int end = drawText(d, f, x, y, text, ink, p::Ground);
    if (end < x + w) {
        d.fillRect(end, y, x + w - end, f.height, p::Ground);
    }
}

} // namespace

void StandbyScreen::drawStatus(Display &d, const StandbyState &s)
{
    // The status row on black, a rule under it; the caller draws the
    // battery at its right end.
    d.fillRect(0, 0, d.width(), StatusH - 1, p::Ground);
    d.fillRect(0, StatusH - 1, d.width(), 1, p::Rule);
    int x = 2;
    if (s.transmitting) {
        x = tag(d, x, "TX", p::Transmit, p::Text);
    } else if (s.receiving) {
        x = tag(d, x, "RX", p::Receive, p::Text);
    }
    if (s.locked) {
        x = tag(d, x, "LOCK", p::Warn, p::Ground);
    }
    // GPS: amber while asleep between fixes, green with a fix, grey while
    // searching, red without data.
    if (s.gps.on) {
        if (s.gps.asleep) {
            tag(d, x, "GPS", p::Warn, p::Ground);
        } else if (s.gps.fix.valid) {
            tag(d, x, "GPS", p::Receive, p::Text);
        } else if (s.gps.receiving) {
            tag(d, x, "GPS", p::Rule, p::Dim);
        } else {
            tag(d, x, "GPS", p::Transmit, p::Text);
        }
    }
    // Volume, left of the battery: a speaker mark and the step.
    int right = batteryX(d);
    if (s.volume >= 0) {
        lib::StaticString<8> v;
        v << "VOL " << s.volume;
        right -= textWidth(Small, v.view()) + 4;
        drawText(d, Small, right, 2, v.view(), s.volume == 0 ? p::Warn : p::Dim, p::Ground);
    }
    // The local time, left of the volume.
    if (!s.clock.empty()) {
        right -= textWidth(Small, s.clock) + 6;
        drawText(d, Small, right, 2, s.clock, p::Text, p::Ground);
    }
}

void StandbyScreen::draw(Display &d, const ChannelSelector &sel, const StandbyState &s)
{
    const int w = d.width();
    const int h = d.height();
    drawStatus(d, s);
    d.fillRect(0, StatusH, w, h - StatusH - HintH, p::Ground);

    // Zone line: zone name, or VFO, and the position.
    const int zoneY = StatusH + 3;
    lib::StaticString<24> right;
    if (sel.vfoMode()) {
        drawText(d, Body, 4, zoneY, "VFO", p::Dim, p::Ground);
    } else {
        drawText(d, Body, 4, zoneY, sel.zoneName(), p::Dim, p::Ground);
        right << sel.position() << '/' << sel.zoneSize();
        drawText(d, Body, w - 4 - textWidth(Body, right.view()), zoneY, right.view(), p::Dim, p::Ground);
    }

    const auto &c = sel.current();
    const int nameY = zoneY + Body.height + 4;
    const int freqY = nameY + Large.height + 2;
    if (!s.entry.empty()) {
        const auto e = entryText(s.entry);
        drawText(d, Large, (w - textWidth(Large, e.view())) / 2, nameY, e.view(), p::Text, p::Ground);
        drawText(d, Body, 4, freqY + 4, "MHz  Menu: tune  Exit: cancel", p::Dim, p::Ground);
    } else if (!c) {
        drawText(d, Medium, 4, nameY, "No channel", p::Dim, p::Ground);
    } else {
        const auto f = mhz(c->receiveHz);
        if (sel.vfoMode() || c->name[0] == '\0') {
            drawText(d, Large, (w - textWidth(Large, f.view())) / 2, nameY, f.view(), p::Text, p::Ground);
        } else {
            const std::string_view name(c->name);
            const Font &nf = textWidth(Large, name) <= w - 8 ? Large : Medium;
            drawText(d, nf, (w - textWidth(nf, name)) / 2, nameY + (Large.height - nf.height) / 2, name, p::Text,
                p::Ground);
            lib::StaticString<16> fl;
            fl << f.view() << " MHz";
            drawText(d, Medium, (w - textWidth(Medium, fl.view())) / 2, freqY, fl.view(), p::Text, p::Ground);
        }

        // Details: mode and width, tones, power, offset.
        lib::StaticString<40> det;
        if (!c->analog()) {
            det << "DMR";
        } else {
            det << "FM " << (c->wide ? "25k" : "12.5k");
            if (c->transmitTone.kind != Tone::Kind::None) {
                det << "  T" << toneText(c->transmitTone).view();
            }
            if (c->receiveTone.kind != Tone::Kind::None) {
                det << "  R" << toneText(c->receiveTone).view();
            }
        }
        const int detY = freqY + Medium.height + 6;
        line(d, Body, 4, detY, w - 8, det.view(), p::Dim);
        lib::StaticString<40> det2;
        det2 << powerText(c->power);
        if (c->transmitHz != c->receiveHz) {
            const bool up = c->transmitHz > c->receiveHz;
            const std::uint32_t off = up ? c->transmitHz - c->receiveHz : c->receiveHz - c->transmitHz;
            det2 << "  " << (up ? '+' : '-') << mhz(off).view();
        }
        if (c->receiveOnly) {
            det2 << "  RX ONLY";
        }
        line(d, Body, 4, detY + Body.height + 1, w - 8, det2.view(), p::Dim);
    }

    // Bottom bar: the quick keys, or a short note.
    d.fillRect(0, h - HintH, w, 1, p::Rule);
    d.fillRect(0, h - HintH + 1, w, HintH - 1, p::Ground);
    const std::string_view hints = s.note.empty() ? s.keys : s.note;
    drawText(d, Small, 3, h - HintH + 2, hints, p::Dim, p::Ground);
}

void drawGpsScreen(Display &d, const GpsState &gps, bool full)
{
    const int w = d.width();
    const int h = d.height();
    const int top = StandbyScreen::StatusH;
    constexpr const char *Labels[] = {"Lat", "Lon", "Alt", "Sats", "Speed", "UTC"};
    const int firstRow = top + Medium.height + 5;
    if (full) {
        d.fillRect(0, top, w, h - top, p::Ground);
        drawText(d, Medium, 4, top + 2, "GPS", p::Text, p::Ground);
        for (int i = 0; i < static_cast<int>(std::size(Labels)); ++i) {
            drawText(d, Body, 4, firstRow + i * (Body.height + 1), Labels[i], p::Dim, p::Ground);
        }
        d.fillRect(0, h - StandbyScreen::HintH, w, 1, p::Rule);
    }

    const services::GpsFix &f = gps.fix;
    std::string_view state = "Off";
    Color stateInk = p::Dim;
    if (gps.on && gps.asleep) {
        state = "Sleeping";
        stateInk = p::Warn;
    } else if (gps.on && f.valid) {
        state = "Fix";
        stateInk = p::Receive;
    } else if (gps.on && gps.receiving) {
        state = "Searching";
    } else if (gps.on) {
        state = "No data";
        stateInk = p::Transmit;
    }
    // Right-aligned in a field as wide as the longest state, so the spaces
    // cover a longer word drawn before.
    constexpr std::size_t StateChars = 9;
    lib::StaticString<StateChars + 1> padded;
    for (std::size_t i = state.size(); i < StateChars; ++i) {
        padded << ' ';
    }
    padded << state;
    drawText(d, Medium, w - 4 - textWidth(Medium, padded.view()), top + 2, padded.view(), stateInk, p::Ground);

    int y = firstRow;
    auto value = [&](std::string_view text) {
        line(d, Body, 46, y, w - 50, text, p::Text);
        y += Body.height + 1;
    };
    const bool position = gps.on && f.valid;
    value(position ? latitudeText(f.latitudeE7).view() : "-");
    value(position ? longitudeText(f.longitudeE7).view() : "-");
    lib::StaticString<24> v;
    if (position) {
        v << f.altitudeDm / 10 << " m";
    } else {
        v << "-";
    }
    value(v.view());
    v.clear();
    if (gps.on) {
        v << f.satellitesUsed << " of " << f.satellitesInView;
    } else {
        v << "-";
    }
    value(v.view());
    v.clear();
    if (position) {
        v << f.speedDeciKmh / 10 << '.' << f.speedDeciKmh % 10 << " km/h " << f.courseDeciDeg / 10 << " deg";
    } else {
        v << "-";
    }
    value(v.view());
    v.clear();
    if (gps.on && f.timeValid) {
        v << lib::zeroPadded(f.hour, 2) << ':' << lib::zeroPadded(f.minute, 2) << ':' << lib::zeroPadded(f.second, 2);
        if (f.dateValid) {
            v << ' ' << f.year << '-' << lib::zeroPadded(f.month, 2) << '-' << lib::zeroPadded(f.day, 2);
        }
    } else {
        v << "-";
    }
    value(v.view());

    line(d, Small, 3, h - StandbyScreen::HintH + 2, w - 6, "Exit: back", p::Dim);
}

void ListPicker::open(std::string_view title)
{
    m_count = 0;
    m_cursor = 0;
    copyText(m_title, title);
}

void ListPicker::add(std::uint8_t id, std::string_view text, std::string_view value)
{
    if (m_count >= MaxItems) {
        return;
    }
    m_ids[m_count] = id;
    copyText(m_text[m_count], text);
    copyText(m_value[m_count], value);
    ++m_count;
}

void ListPicker::select(std::uint8_t id)
{
    for (std::size_t i = 0; i < m_count; ++i) {
        if (m_ids[i] == id) {
            m_cursor = i;
        }
    }
}

void ListPicker::move(int steps)
{
    if (m_count == 0) {
        return;
    }
    const int n = static_cast<int>(m_count);
    m_cursor = static_cast<std::size_t>(((static_cast<int>(m_cursor) + steps) % n + n) % n);
}

void ListPicker::draw(Display &d, int top)
{
    const int w = d.width();
    const int rowH = Body.height + 2;
    d.fillRect(0, top, w, rowH - 1, p::Ground);
    drawText(d, Body, 4, top + 1, m_title, p::Dim, p::Ground);
    d.fillRect(0, top + rowH - 1, w, 1, p::Rule);
    const int first = m_cursor < static_cast<std::size_t>(Rows) ? 0 : static_cast<int>(m_cursor) - Rows + 1;
    for (int r = 0; r < Rows; ++r) {
        const int y = top + rowH * (r + 1);
        const int i = first + r;
        if (i >= static_cast<int>(m_count)) {
            d.fillRect(0, y, w, rowH, p::Ground);
            continue;
        }
        const bool on = static_cast<std::size_t>(i) == m_cursor;
        const Color fill = on ? p::Text : p::Ground;
        d.fillRect(0, y, w, rowH, fill);
        drawText(d, Body, 6, y + 1, m_text[i], on ? p::Ground : p::Text, fill);
        const std::string_view value(m_value[i]);
        if (!value.empty()) {
            drawText(d, Body, w - 6 - textWidth(Body, value), y + 1, value, on ? p::Ground : p::Dim, fill);
        }
    }
    const int end = top + rowH * (Rows + 1);
    if (end < d.height()) {
        d.fillRect(0, end, w, d.height() - end, p::Ground);
    }
}

} // namespace dmm::app::ui
