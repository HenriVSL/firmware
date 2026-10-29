// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tommi Tauriainen

#include "standby.h"

#include "dmm/lib/format.h"
#include "dmm/services/font.h"

#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <iterator>
#include <utility>

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

// At most @p chars characters of @p s.
std::string_view cut(std::string_view s, int chars)
{
    return {s.data(), chars <= 0 ? 0 : std::min(s.size(), static_cast<std::size_t>(chars))};
}

// A filled rectangle; nothing when it is empty.
void fillArea(Display &d, int x, int y, int w, int h, Color c)
{
    if (w > 0 && h > 0) {
        d.fillRect(x, y, w, h, c);
    }
}

// One text of a band: where it goes, in which font and colour.
struct Piece
{
    int x;
    int y;
    std::string_view text;
    const Font *font;
    Color ink;
};

// A band of the screen, rows [y, y + h) and columns [x0, x1) (to the
// right edge when x1 is 0): each piece of text is drawn with its own
// background first, then every other pixel of the band is painted
// @p ground. Nothing is cleared before it is drawn over, so a redraw does
// not flicker, and text of any size fits as long as it lies inside the
// band. The pieces must not overlap side by side.
void band(Display &d, int y, int h, std::initializer_list<Piece> pieces, Color ground = p::Ground, int x0 = 0,
    int x1 = 0)
{
    const int w = x1 > 0 ? x1 : d.width();
    Piece sorted[4];
    std::size_t n = 0;
    for (const Piece &pc : pieces) {
        if (!pc.text.empty() && n < std::size(sorted)) {
            sorted[n++] = pc;
        }
    }
    // Left to right; at most a few pieces.
    for (std::size_t i = 1; i < n; ++i) {
        for (std::size_t k = i; k > 0 && sorted[k].x < sorted[k - 1].x; --k) {
            std::swap(sorted[k], sorted[k - 1]);
        }
    }
    int cursor = x0;
    for (std::size_t i = 0; i < n; ++i) {
        const Piece &pc = sorted[i];
        const int x = std::max(pc.x, cursor);
        const int end = std::min(drawText(d, *pc.font, x, pc.y, pc.text, pc.ink, ground), w);
        fillArea(d, cursor, y, x - cursor, h, ground);
        fillArea(d, x, y, end - x, pc.y - y, ground);
        fillArea(d, x, pc.y + pc.font->height, end - x, y + h - pc.y - pc.font->height, ground);
        cursor = end;
    }
    fillArea(d, cursor, y, w - cursor, h, ground);
}

// @p text from @p x, the rest of a @p w wide row painted ground.
void line(Display &d, const Font &f, int x, int y, int w, std::string_view text, Color ink)
{
    const int end = drawText(d, f, x, y, text, ink, p::Ground);
    if (end < x + w) {
        d.fillRect(end, y, x + w - end, f.height, p::Ground);
    }
}

// The working screen's rows, top to bottom.
constexpr int LabelY = StandbyScreen::StatusH; // label and time
constexpr int NameY = LabelY + 19;             // talkgroup or channel, large
constexpr int PanelY = NameY + 28;             // frequency and slot, on a panel
constexpr int SettingsY = PanelY + 22;         // power and volume
constexpr int InfoY = SettingsY + 18;          // roaming or channel details
// The right half of the working screen, from the volume and the time.
constexpr int RightX = 96;

// The text centred across the screen.
int centred(const Display &d, const Font &f, std::string_view s)
{
    return (d.width() - textWidth(f, s)) / 2;
}

// "0:07", "12:30": minutes and seconds.
lib::StaticString<12> minutesText(std::uint32_t seconds)
{
    lib::StaticString<12> t;
    t << seconds / 60 << ':' << static_cast<char>('0' + seconds % 60 / 10) << static_cast<char>('0' + seconds % 10);
    return t;
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

} // namespace

void StandbyScreen::drawStatus(Display &d, const StandbyState &s)
{
    // The status strip: marks for what matters now on the left; the caller
    // draws the battery at its right end. The rule under it turns red while
    // transmitting and green while receiving. Each part is drawn in place,
    // with only the gaps painted ground, so a redraw does not flicker.
    const int rowH = StatusH - 1;
    int cursor = 0;
    auto gapTo = [&](int x) {
        fillArea(d, cursor, 0, x - cursor, rowH, p::Ground);
        cursor = std::max(cursor, x);
    };
    auto mark = [&](std::string_view text, Color ink) {
        gapTo(cursor + 4);
        cursor = drawText(d, Small, cursor, 2, text, ink, p::Ground);
        fillArea(d, cursor - textWidth(Small, text), 0, textWidth(Small, text), 2, p::Ground);
        fillArea(d, cursor - textWidth(Small, text), 2 + Small.height, textWidth(Small, text),
            rowH - 2 - Small.height, p::Ground);
    };
    if (s.locked) {
        mark("LOCK", p::Warn);
    }
    // GPS: only while it is on and still looking for a fix.
    if (s.gps.on && !s.gps.asleep && !s.gps.fix.valid) {
        mark("GPS..", p::Warn);
    }
    if (s.beacons) {
        mark("APRS", p::Beacon);
    }
    gapTo(batteryX(d));
    d.fillRect(0, StatusH - 1, d.width(), 1, s.transmitting ? p::Transmit : s.receiving ? p::Receive
                                                                                        : p::Rule);
}

void StandbyScreen::drawVolume(Display &d, const StandbyState &s)
{
    lib::StaticString<4> v;
    if (s.volume >= 0) {
        v << s.volume;
    }
    band(d, SettingsY, InfoY - SettingsY - 1,
        {{RightX, SettingsY + 5, "VOL", &Small, p::Dim},
            {RightX + 22, SettingsY + 3, v.view(), &Body, s.volume == 0 ? p::Warn : p::Text}},
        p::Ground, RightX);
}

void StandbyScreen::drawClock(Display &d, const StandbyState &s)
{
    const int w = d.width();
    band(d, LabelY, NameY - LabelY, {{w - 4 - textWidth(Medium, s.clock), LabelY + 2, s.clock, &Medium, p::Text}},
        p::Ground, RightX);
}

void StandbyScreen::draw(Display &d, const ChannelSelector &sel, const StandbyState &s)
{
    const int w = d.width();
    const int h = d.height();
    drawStatus(d, s);
    const auto &c = sel.current();

    // What the screen is about, with the time on the right.
    std::string_view label = "CHANNEL";
    if (!s.entry.empty()) {
        label = "ENTER MHz";
    } else if (sel.vfoMode()) {
        label = "VFO";
    } else if (c && !c->analog() && !s.talkgroup.empty()) {
        label = "TALKGROUP";
    }
    band(d, LabelY, NameY - LabelY, {{4, LabelY + 4, label, &Small, p::Dim}}, p::Ground, 0, RightX);
    drawClock(d, s);

    // The one thing that matters most, large: the talkgroup, the channel's
    // name, or the frequency; a name beside it when there is room.
    std::string_view primary;
    std::string_view secondary;
    lib::StaticString<12> f;
    if (!s.entry.empty()) {
        f = entryText(s.entry);
        primary = f.view();
    } else if (c) {
        f = mhz(c->receiveHz);
        if (label == "TALKGROUP") {
            primary = s.talkgroup;
            secondary = s.talkgroupName;
        } else if (sel.vfoMode() || c->name[0] == '\0') {
            primary = f.view();
        } else {
            primary = c->name;
        }
    } else {
        primary = "No channel";
    }
    const int nameH = PanelY - NameY - 1;
    const bool large = textWidth(Large, primary) <= w - 8;
    const Font &pf = large ? Large : Medium;
    const int py = NameY + (Large.height - pf.height) / 2 + 1;
    const int sx = 4 + textWidth(pf, primary) + 8;
    const bool besides = !secondary.empty() && sx + textWidth(Body, secondary) <= w - 4;
    band(d, NameY, nameH,
        {{4, py, primary, &pf, p::Text},
            {sx, NameY + Large.height - Body.height, besides ? secondary : std::string_view{}, &Body, p::Dim}});
    d.fillRect(0, PanelY - 1, w, 1, p::Rule);

    // Frequency and slot on a panel.
    lib::StaticString<24> freq;
    lib::StaticString<8> slot;
    lib::StaticString<16> slotNote;
    if (s.entry.empty() && c && s.roaming && s.roamSearching) {
        freq << "Searching";
    } else if (s.entry.empty() && c) {
        freq << mhz(s.roaming && s.roamHz != 0 ? s.roamHz : c->receiveHz).view();
        if (!c->analog()) {
            slot << "TS" << c->dmr.slot;
            slotNote << "CC" << (s.roaming ? s.roamColourCode : c->dmr.colourCode);
        } else {
            slot << "FM";
            slotNote << (c->wide ? "25k" : "12.5k");
        }
    }
    band(d, PanelY, SettingsY - PanelY,
        {{4, PanelY + 7, freq.view().empty() ? std::string_view{} : "MHz", &Small, p::Dim},
            {26, PanelY + 5, freq.view(), &Body, p::Text}, {RightX, PanelY + 5, slot.view(), &Body, p::Text},
            {RightX + 24, PanelY + 8, slotNote.view(), &Small, p::Dim}},
        palette::Panel);

    // Power and volume on one line.
    band(d, SettingsY, InfoY - SettingsY - 1,
        {{4, SettingsY + 5, c ? "PWR" : "", &Small, p::Dim},
            {26, SettingsY + 3, c ? std::string_view(powerText(c->power)) : std::string_view{}, &Body, p::Text}},
        p::Ground, 0, RightX);
    drawVolume(d, s);
    d.fillRect(0, InfoY - 1, w, 1, p::Rule);

    // Details: the repeater a roaming channel works through; the channel
    // and its place in the zone; tones and offset on FM.
    lib::StaticString<40> info;
    if (!s.entry.empty()) {
        info << "Menu: tune  Exit: cancel";
    } else if (c && s.roaming) {
        if (s.roamSearching) {
            info << "Looking for a repeater";
        } else if (!s.roamSite.empty()) {
            info << "via " << s.roamSite;
        } else {
            info << "via the channel's repeater";
        }
    } else if (c && c->analog()) {
        if (c->transmitTone.kind != Tone::Kind::None) {
            info << "T" << toneText(c->transmitTone).view() << "  ";
        }
        if (c->receiveTone.kind != Tone::Kind::None) {
            info << "R" << toneText(c->receiveTone).view() << "  ";
        }
        if (c->transmitHz != c->receiveHz) {
            const bool up = c->transmitHz > c->receiveHz;
            const std::uint32_t off = up ? c->transmitHz - c->receiveHz : c->receiveHz - c->transmitHz;
            info << (up ? '+' : '-') << mhz(off).view();
        }
    } else if (c && !sel.vfoMode()) {
        // Under a talkgroup, the channel; under the channel, its zone.
        info << cut(label == "TALKGROUP" ? std::string_view(c->name) : sel.zoneName(), 16) << "  " << sel.position()
             << '/' << sel.zoneSize();
    }
    if (c && c->receiveOnly) {
        info << "  RX ONLY";
    }
    band(d, InfoY, h - HintH - InfoY, {{4, InfoY + 5, info.view(), &Small, p::Dim}});

    // Bottom bar: the quick keys, or a short note.
    d.fillRect(0, h - HintH, w, 1, p::Rule);
    const std::string_view hints = s.note.empty() ? s.keys : s.note;
    band(d, h - HintH + 1, HintH - 1, {{3, h - HintH + 2, hints, &Small, p::Dim}});
}

// The call and transmit screens' rows.
constexpr int StateY = StandbyScreen::StatusH; // "RECEIVING" or "TRANSMITTING"
constexpr int WhoY = StateY + 16;              // the caller or the talkgroup, large
constexpr int WhoNameY = WhoY + 28;            // their name
constexpr int WhoIdY = WhoNameY + 14;          // the caller's ID, small
constexpr int TimeY = WhoIdY + 11;             // the running time
constexpr int TimeEndY = TimeY + 26;

void StandbyScreen::drawCall(Display &d, const StandbyState &s, const CallInfo &c)
{
    const int w = d.width();
    const int h = d.height();
    drawStatus(d, s);

    // What is heard and to whom, green while it lasts.
    lib::StaticString<32> head;
    head << (c.active ? "RECEIVING  " : "LAST CALL  ");
    if (c.allCall) {
        head << "ALL";
    } else if (c.group) {
        head << "TG " << c.targetId;
    } else {
        head << "PRIVATE";
    }
    band(d, StateY, WhoY - StateY,
        {{centred(d, Small, head.view()), StateY + 4, head.view(), &Small, c.active ? p::Receive : p::Dim}});

    // The caller: callsign or name large, or the ID when neither is known;
    // the user's name and the ID under it.
    lib::StaticString<12> callerId;
    callerId << c.callerId;
    const bool known = !c.callerHeadline.empty();
    const std::string_view caller = known ? c.callerHeadline : callerId.view();
    const Font &cf = textWidth(Large, caller) <= w - 8 ? Large : Medium;
    band(d, WhoY, WhoNameY - WhoY,
        {{centred(d, cf, caller), WhoY + (Large.height - cf.height) / 2, caller, &cf, c.active ? p::Text : p::Dim}});
    const std::string_view name = cut(c.callerName, (w - 8) / Body.width);
    band(d, WhoNameY, WhoIdY - WhoNameY, {{centred(d, Body, name), WhoNameY, name, &Body, p::Dim}});
    lib::StaticString<16> id;
    if (known) {
        id << "ID " << c.callerId;
    }
    band(d, WhoIdY, TimeY - WhoIdY, {{centred(d, Small, id.view()), WhoIdY + 1, id.view(), &Small, p::Rule}});
    drawCallTime(d, c);
    band(d, TimeEndY, h - HintH - TimeEndY, {});

    // The bottom bar: the slot and the channel.
    d.fillRect(0, h - HintH, w, 1, p::Rule);
    lib::StaticString<32> where;
    if (c.slot != 0) {
        where << "TS" << c.slot << "   ";
    }
    where << cut(c.channel, 20);
    band(d, h - HintH + 1, HintH - 1, {{3, h - HintH + 2, where.view(), &Small, p::Dim}});
}

void StandbyScreen::drawCallTime(Display &d, const CallInfo &c)
{
    const auto t = minutesText(c.seconds);
    band(d, TimeY, TimeEndY - TimeY,
        {{centred(d, Medium, t.view()), TimeY + 5, t.view(), &Medium, c.active ? p::Text : p::Dim}});
}

void StandbyScreen::drawTransmit(Display &d, const StandbyState &s, const TransmitInfo &t)
{
    const int w = d.width();
    const int h = d.height();
    drawStatus(d, s);
    band(d, StateY, WhoY - StateY,
        {{centred(d, Small, "TRANSMITTING"), StateY + 4, "TRANSMITTING", &Small, p::Transmit}});
    const Font &tf = textWidth(Large, t.target) <= w - 8 ? Large : Medium;
    band(d, WhoY, WhoNameY - WhoY,
        {{centred(d, tf, t.target), WhoY + (Large.height - tf.height) / 2, t.target, &tf, p::Text}});
    const std::string_view name = cut(t.name, (w - 8) / Body.width);
    band(d, WhoNameY, TimeY - WhoNameY, {{centred(d, Body, name), WhoNameY, name, &Body, p::Dim}});
    drawTransmitTime(d, t);
    band(d, TimeEndY, h - HintH - TimeEndY, {});
    d.fillRect(0, h - HintH, w, 1, p::Rule);
    lib::StaticString<32> how;
    if (t.slot != 0) {
        how << "TS" << t.slot << "   ";
    }
    how << "POWER " << t.power;
    band(d, h - HintH + 1, HintH - 1, {{3, h - HintH + 2, how.view(), &Small, p::Dim}});
}

void StandbyScreen::drawTransmitTime(Display &d, const TransmitInfo &t)
{
    const auto time = minutesText(t.seconds);
    band(d, TimeY, TimeEndY - TimeY, {{centred(d, Medium, time.view()), TimeY + 5, time.view(), &Medium, p::Text}});
}

// The activity list: a title, rows of time and text, and a hint bar.
constexpr int LogTop = StandbyScreen::StatusH + 15;

void drawLogFrame(Display &d, std::string_view title, std::string_view note, std::string_view hint)
{
    const int w = d.width();
    const int h = d.height();
    band(d, StandbyScreen::StatusH, LogTop - StandbyScreen::StatusH,
        {{4, StandbyScreen::StatusH + 2, title, &Body, p::Text},
            {w - 4 - textWidth(Small, note), StandbyScreen::StatusH + 5, note, &Small, p::Rule}});
    d.fillRect(0, h - StandbyScreen::HintH, w, 1, p::Rule);
    band(d, h - StandbyScreen::HintH + 1, StandbyScreen::HintH - 1,
        {{3, h - StandbyScreen::HintH + 2, hint, &Small, p::Dim}});
}

void drawLogRow(Display &d, int row, std::string_view time, std::string_view text, Color ink)
{
    const int y = LogTop + row * LogRowH;
    band(d, y, LogRowH, {{4, y + 1, time, &Small, p::Rule}, {34, y + 1, cut(text, 25), &Small, ink}});
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

namespace {

struct Path
{
    float metres;
    float bearingDeg;
};

// Distance and bearing between two positions, on a flat map around them:
// within a few metres per kilometre at the ranges radio reaches.
Path pathBetween(std::int32_t lat1E7, std::int32_t lon1E7, std::int32_t lat2E7, std::int32_t lon2E7)
{
    constexpr float MetresPerE7 = 6'371'000.0f * 3.14159265f / 180.0f / 1e7f;
    const float meanLat = (static_cast<float>(lat1E7) + static_cast<float>(lat2E7)) / 2.0f * (3.14159265f / 180.0f / 1e7f);
    const float north = static_cast<float>(lat2E7 - lat1E7) * MetresPerE7;
    // Across the date line the short way round.
    std::int64_t dLon = static_cast<std::int64_t>(lon2E7) - lon1E7;
    if (dLon > 1'800'000'000) {
        dLon -= 3'600'000'000;
    } else if (dLon < -1'800'000'000) {
        dLon += 3'600'000'000;
    }
    const float east = static_cast<float>(dLon) * MetresPerE7 * std::cos(meanLat);
    float bearing = std::atan2(east, north) * (180.0f / 3.14159265f);
    if (bearing < 0) {
        bearing += 360;
    }
    return {std::sqrt(north * north + east * east), bearing};
}

const char *compassPoint(float degrees)
{
    constexpr const char *Points[] = {"N", "NE", "E", "SE", "S", "SW", "W", "NW"};
    return Points[static_cast<int>((degrees + 22.5f) / 45.0f) % 8];
}

} // namespace

lib::StaticString<10> distanceText(const GpsState &gps, const aprs::HeardStation &s)
{
    lib::StaticString<10> t;
    if (!gps.fix.valid || !s.hasPosition) {
        return t;
    }
    const Path path = pathBetween(gps.fix.latitudeE7, gps.fix.longitudeE7, s.latitudeE7, s.longitudeE7);
    if (path.metres < 1000) {
        t << static_cast<int>(path.metres) << " m";
    } else if (path.metres < 100'000) {
        const int tenths = static_cast<int>(path.metres / 100);
        t << tenths / 10 << '.' << tenths % 10 << " km";
    } else {
        t << static_cast<int>(path.metres / 1000) << " km";
    }
    return t;
}

lib::StaticString<8> ageText(std::uint32_t thenMs, std::uint32_t nowMs)
{
    lib::StaticString<8> t;
    const std::uint32_t s = (nowMs - thenMs) / 1000;
    if (s < 60) {
        t << s << " s";
    } else if (s < 3600) {
        t << s / 60 << " min";
    } else {
        t << s / 3600 << " h";
    }
    return t;
}

void drawAprsStation(Display &d, const aprs::HeardStation &s, const GpsState &gps, std::uint32_t nowMs)
{
    const int w = d.width();
    const int h = d.height();
    const int top = StandbyScreen::StatusH;
    constexpr const char *Labels[] = {"Lat", "Lon", "Dist", "Alt", "Speed", "Text"};
    const int firstRow = top + Medium.height + 5;
    d.fillRect(0, top, w, h - top, p::Ground);
    drawText(d, Medium, 4, top + 2, s.call, p::Text, p::Ground);
    const lib::StaticString<8> age = ageText(s.heardAtMs, nowMs);
    drawText(d, Body, w - 4 - textWidth(Body, age.view()), top + 4, age.view(), p::Dim, p::Ground);
    for (int i = 0; i < static_cast<int>(std::size(Labels)); ++i) {
        drawText(d, Body, 4, firstRow + i * (Body.height + 1), Labels[i], p::Dim, p::Ground);
    }
    d.fillRect(0, h - StandbyScreen::HintH, w, 1, p::Rule);

    int y = firstRow;
    auto value = [&](std::string_view text) {
        line(d, Body, 46, y, w - 50, text, p::Text);
        y += Body.height + 1;
    };
    value(s.hasPosition ? latitudeText(s.latitudeE7).view() : "-");
    value(s.hasPosition ? longitudeText(s.longitudeE7).view() : "-");
    lib::StaticString<24> v;
    if (gps.fix.valid && s.hasPosition) {
        const Path path = pathBetween(gps.fix.latitudeE7, gps.fix.longitudeE7, s.latitudeE7, s.longitudeE7);
        v << distanceText(gps, s).view() << ' ' << static_cast<int>(path.bearingDeg + 0.5f) % 360 << ' ' << compassPoint(path.bearingDeg);
    } else {
        v << "-";
    }
    value(v.view());
    v.clear();
    if (s.hasAltitude) {
        v << s.altitudeM << " m";
    } else {
        v << "-";
    }
    value(v.view());
    v.clear();
    if (s.hasCourse) {
        // 1 knot = 1.852 km/h.
        v << (s.speedKnots * 1852 + 500) / 1000 << " km/h " << s.courseDeg << " deg";
    } else {
        v << "-";
    }
    value(v.view());
    value(s.text[0] != '\0' ? std::string_view(s.text) : "-");

    v.clear();
    v << (s.via[0] != '\0' ? "via " : "direct") << s.via << ", " << s.packets << (s.packets == 1 ? " packet" : " packets");
    line(d, Small, 3, h - StandbyScreen::HintH + 2, w - 6, v.view(), p::Dim);
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
