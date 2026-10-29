// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Henri Lavikainen

// Draws menu and codeplug editing concepts with the radio's own fonts at
// the panel's 160 x 128 and saves them as PNG files, scaled up. Menu text
// is the body font or larger; only the status strip and the hint bar use
// the small font.
// usage: menu-concepts <output folder>

#include "dmm/services/display.h"
#include "dmm/services/font.h"

#include <QDir>
#include <QImage>
#include <QString>

#include <cstdio>
#include <string_view>

namespace {

using dmm::services::Color;
using dmm::services::Font;
using dmm::services::rgb;
namespace fonts = dmm::services::fonts;

const Font &Small = fonts::Spleen5x8;
const Font &Body = fonts::Spleen6x12;
const Font &Medium = fonts::Spleen8x16;
const Font &Large = fonts::Spleen12x24;

class ImageDisplay final : public dmm::services::Display
{
public:
    ImageDisplay() : m_image(160, 128, QImage::Format_RGB32) { m_image.fill(Qt::black); }
    void start() override {}
    int width() const override { return 160; }
    int height() const override { return 128; }
    void fillRect(int x, int y, int w, int h, Color c) override
    {
        for (int j = y; j < y + h; ++j) {
            for (int i = x; i < x + w; ++i) {
                put(i, j, c);
            }
        }
    }
    void drawBitmap(int x, int y, int w, int h, const std::uint8_t *rows, int stride, Color fg, Color bg) override
    {
        for (int j = 0; j < h; ++j) {
            for (int i = 0; i < w; ++i) {
                const bool on = (rows[j * stride + i / 8] >> (7 - i % 8)) & 1;
                put(x + i, y + j, on ? fg : bg);
            }
        }
    }
    void setBacklight(int) override {}
    void setPanelOn(bool) override {}
    void save(const QString &path) const
    {
        m_image.scaled(160 * 4, 128 * 4, Qt::IgnoreAspectRatio, Qt::FastTransformation).save(path);
    }

private:
    void put(int x, int y, Color c)
    {
        if (x < 0 || y < 0 || x >= 160 || y >= 128) {
            return;
        }
        const int r = (c >> 11 & 0x1F) * 255 / 31;
        const int g = (c >> 5 & 0x3F) * 255 / 63;
        const int b = (c & 0x1F) * 255 / 31;
        m_image.setPixel(x, y, qRgb(r, g, b));
    }
    QImage m_image;
};

namespace p {
constexpr Color Ground = rgb(0, 0, 0);
constexpr Color Text = rgb(255, 255, 255);
constexpr Color Dim = rgb(150, 150, 150);
constexpr Color Rule = rgb(70, 70, 70);
constexpr Color Transmit = rgb(210, 40, 30);
constexpr Color Receive = rgb(40, 170, 60);
constexpr Color Warn = rgb(230, 160, 0);
constexpr Color Beacon = rgb(40, 100, 200);
constexpr Color Panel = rgb(28, 28, 32);
constexpr Color Lift = rgb(52, 52, 60);
} // namespace p

int width(const Font &f, std::string_view s)
{
    return static_cast<int>(s.size()) * f.width;
}
void text(ImageDisplay &d, const Font &f, int x, int y, std::string_view s, Color fg, Color bg)
{
    dmm::services::drawText(d, f, x, y, s, fg, bg);
}
void right(ImageDisplay &d, const Font &f, int xr, int y, std::string_view s, Color fg, Color bg)
{
    text(d, f, xr - width(f, s), y, s, fg, bg);
}
void centre(ImageDisplay &d, const Font &f, int y, std::string_view s, Color fg, Color bg, int x0 = 0, int w = 160)
{
    text(d, f, x0 + (w - width(f, s)) / 2, y, s, fg, bg);
}
void box(ImageDisplay &d, int x, int y, int w, int h, Color c)
{
    d.fillRect(x, y, w, 1, c);
    d.fillRect(x, y + h - 1, w, 1, c);
    d.fillRect(x, y, 1, h, c);
    d.fillRect(x + w - 1, y, 1, h, c);
}

// The status strip as on the radio: marks on the left, the battery.
void strip(ImageDisplay &d)
{
    d.fillRect(0, 0, 160, 128, p::Ground);
    text(d, Small, 3, 2, "APRS", p::Beacon, p::Ground);
    const int x = 160 - 5 - 21;
    d.fillRect(x, 1, 21, 9, p::Text);
    d.fillRect(x + 21, 3, 2, 5, p::Text);
    d.fillRect(x + 15, 2, 5, 7, p::Ground);
    d.fillRect(0, 10, 160, 1, p::Rule);
}

void hint(ImageDisplay &d, std::string_view s)
{
    d.fillRect(0, 117, 160, 1, p::Rule);
    text(d, Small, 3, 119, s, p::Dim, p::Ground);
}

// A title band: where we are, and a note on the right.
void title(ImageDisplay &d, std::string_view where, std::string_view note)
{
    d.fillRect(0, 11, 160, 13, p::Panel);
    text(d, Body, 4, 12, where, p::Dim, p::Panel);
    right(d, Body, 156, 12, note, p::Rule, p::Panel);
}

// A scroll bar at the right edge for @p shown of @p total rows from @p first.
void scrollBar(ImageDisplay &d, int top, int h, int first, int shown, int total)
{
    if (total <= shown) {
        return;
    }
    d.fillRect(157, top, 2, h, p::Panel);
    const int bh = h * shown / total;
    d.fillRect(157, top + (h - bh) * first / (total - shown), 2, bh, p::Dim);
}

struct Row
{
    std::string_view label;
    std::string_view value;
    bool more = false; // opens more
};

// --- A: Numbered list. Seven 13-pixel rows of the body font, keypad
// numbers to jump, values edited in place. ---
void numberedRows(ImageDisplay &d, const Row *rows, int count, int chosen, bool editing, int first = 0,
    int total = 0)
{
    for (int i = 0; i < count && i < 7; ++i) {
        const int y = 24 + i * 13;
        const bool on = i == chosen;
        const Color ground = on && !editing ? p::Text : p::Ground;
        const Color ink = on && !editing ? p::Ground : p::Text;
        d.fillRect(0, y, 156, 13, ground);
        char n[2] = {static_cast<char>('1' + first + i), 0};
        text(d, Body, 4, y + 1, n, on && !editing ? p::Rule : p::Rule, ground);
        text(d, Body, 16, y + 1, rows[i].label, ink, ground);
        if (on && editing) {
            const int vw = width(Body, rows[i].value) + 20;
            d.fillRect(154 - vw, y, vw, 13, p::Warn);
            text(d, Body, 154 - vw + 3, y + 1, "<", p::Ground, p::Warn);
            text(d, Body, 154 - vw + 10, y + 1, rows[i].value, p::Ground, p::Warn);
            text(d, Body, 154 - 9, y + 1, ">", p::Ground, p::Warn);
        } else if (rows[i].more) {
            right(d, Body, 152, y + 1, rows[i].value.empty() ? ">" : rows[i].value, on ? p::Ground : p::Dim, ground);
        } else {
            right(d, Body, 152, y + 1, rows[i].value, on ? p::Ground : p::Dim, ground);
        }
    }
    scrollBar(d, 24, 91, first, 7, total);
}

const Row TopRows[] = {
    {"Channel", "", true},
    {"Roaming", "On", true},
    {"Position", "GPS fix", true},
    {"Radio", "", true},
    {"Codeplug", "", true},
    {"Tools", "", true},
};
const Row ChannelRows[] = {
    {"Squelch", "3"},
    {"Squelch type", "Carrier"},
    {"Power", "Low"},
    {"Freq offset", "+2 kHz"},
    {"Edit channel", "", true},
    {"Copy to new", "", true},
    {"Delete", "", true},
};

void aTop(ImageDisplay &d)
{
    strip(d);
    title(d, "Menu", "");
    numberedRows(d, TopRows, 6, 0, false);
    hint(d, "1-6 JUMP MENU OPEN EXIT BACK");
}
void aChannel(ImageDisplay &d)
{
    strip(d);
    title(d, "Menu > Channel", "1/7");
    numberedRows(d, ChannelRows, 7, 0, false);
    hint(d, "1-7 JUMP MENU EDIT EXIT BACK");
}
void aEditing(ImageDisplay &d)
{
    strip(d);
    title(d, "Menu > Channel", "1/7");
    numberedRows(d, ChannelRows, 7, 2, true);
    hint(d, "KNOB CHANGE MENU SET EXIT UNDO");
}

// --- B: Tiles at the top, then lists in the medium font. ---
struct Tile
{
    std::string_view name;
    std::string_view summary;
};
const Tile Tiles[] = {
    {"Channel", "Low  Sq 3"},
    {"Roaming", "On  rep 2"},
    {"Position", "GPS fix"},
    {"Radio", "Light 30s"},
    {"Codeplug", "87 ch"},
    {"Tools", "Log  Keys"},
};
void bTiles(ImageDisplay &d)
{
    strip(d);
    for (int i = 0; i < 6; ++i) {
        const int x = 2 + (i % 2) * 79;
        const int y = 13 + (i / 2) * 34;
        const bool on = i == 2;
        d.fillRect(x, y, 77, 32, on ? p::Lift : p::Panel);
        if (on) {
            box(d, x, y, 77, 32, p::Text);
        }
        char n[2] = {static_cast<char>('1' + i), 0};
        text(d, Small, x + 69, y + 23, n, p::Rule, on ? p::Lift : p::Panel);
        text(d, Medium, x + 4, y + 2, Tiles[i].name, p::Text, on ? p::Lift : p::Panel);
        text(d, Body, x + 4, y + 18, Tiles[i].summary, on ? p::Text : p::Dim, on ? p::Lift : p::Panel);
    }
    hint(d, "1-6/KNOB PICK MENU OPEN");
}

void mediumRows(ImageDisplay &d, const Row *rows, int count, int chosen, int top = 25)
{
    for (int i = 0; i < count; ++i) {
        const int y = top + i * 18;
        const bool on = i == chosen;
        const Color ground = on ? p::Lift : p::Ground;
        d.fillRect(0, y, 156, 18, ground);
        if (on) {
            d.fillRect(0, y, 3, 18, p::Text);
        }
        text(d, Medium, 7, y + 1, rows[i].label, p::Text, ground);
        right(d, Medium, 152, y + 1, rows[i].more && rows[i].value.empty() ? ">" : rows[i].value,
            on ? p::Text : p::Dim, ground);
    }
}
const Row PositionRows[] = {
    {"GPS", "On"},
    {"Systems", "All"},
    {"Beacon", "5 min"},
    {"APRS RX", "On"},
    {"Send now", "", true},
};
void bList(ImageDisplay &d)
{
    strip(d);
    d.fillRect(0, 11, 160, 13, p::Panel);
    text(d, Body, 4, 12, "3 Position", p::Dim, p::Panel);
    right(d, Body, 156, 12, "2/8", p::Rule, p::Panel);
    mediumRows(d, PositionRows, 5, 1);
    scrollBar(d, 25, 90, 0, 5, 8);
    hint(d, "KNOB PICK MENU CHANGE EXIT BACK");
}

// --- C: A short list over a detail panel; values change on their own
// screen, large. ---
void detailList(ImageDisplay &d, const Row *rows, int count, int chosen)
{
    for (int i = 0; i < count; ++i) {
        const int y = 24 + i * 13;
        const bool on = i == chosen;
        const Color ground = on ? p::Text : p::Ground;
        d.fillRect(0, y, 160, 13, ground);
        text(d, Body, 6, y + 1, rows[i].label, on ? p::Ground : p::Text, ground);
        right(d, Body, 154, y + 1, rows[i].more && rows[i].value.empty() ? ">" : rows[i].value,
            on ? p::Ground : p::Dim, ground);
    }
}
const Row RoamRows[] = {
    {"Roaming", "On"},
    {"Roam zone", "Home"},
    {"Status", "", true},
    {"Search now", "", true},
    {"Remember", "On"},
};
void cList(ImageDisplay &d)
{
    strip(d);
    title(d, "Menu > Roaming", "2/5");
    detailList(d, RoamRows, 5, 1);
    d.fillRect(0, 90, 160, 27, p::Panel);
    text(d, Body, 4, 91, "Repeaters to move", p::Dim, p::Panel);
    text(d, Body, 4, 103, "between: 6 in Home", p::Dim, p::Panel);
    hint(d, "KNOB PICK MENU CHANGE EXIT BACK");
}
void cNumber(ImageDisplay &d)
{
    strip(d);
    title(d, "Channel > Squelch", "");
    centre(d, Large, 34, "< 3 >", p::Text, p::Ground);
    // The scale 0..9 with the value marked.
    for (int i = 0; i <= 9; ++i) {
        const int x = 20 + i * 12;
        d.fillRect(x, 70, 10, 8, i <= 3 ? p::Text : p::Panel);
    }
    text(d, Body, 20, 80, "open", p::Rule, p::Ground);
    right(d, Body, 140, 80, "tight", p::Rule, p::Ground);
    centre(d, Body, 98, "Was 3", p::Dim, p::Ground);
    hint(d, "KNOB CHANGE MENU SET EXIT UNDO");
}
void cChoice(ImageDisplay &d)
{
    strip(d);
    title(d, "Channel > Power", "");
    const std::string_view choices[] = {"Low", "Mid", "High", "Turbo"};
    for (int i = 0; i < 4; ++i) {
        const int y = 30 + i * 20;
        const bool on = i == 2;
        d.fillRect(20, y, 120, 18, on ? p::Text : p::Panel);
        centre(d, Medium, y + 1, choices[i], on ? p::Ground : p::Text, on ? p::Text : p::Panel, 20, 120);
        if (i == 0) {
            text(d, Small, 124, y + 5, "*", p::Dim, p::Panel);
        }
    }
    hint(d, "KNOB PICK MENU SET *=NOW");
}

// --- D: Editing the codeplug on the radio. ---
void dPlug(ImageDisplay &d)
{
    strip(d);
    title(d, "Menu > Codeplug", "");
    const Row rows[] = {
        {"Channels", "87", true},
        {"Zones", "8", true},
        {"Contacts", "36", true},
        {"Roam list", "23", true},
        {"RX groups", "1", true},
        {"Undo all", "", true},
        {"Plug info", "", true},
    };
    numberedRows(d, rows, 7, 0, false);
    hint(d, "1-7 JUMP MENU OPEN EXIT BACK");
}
void dChannels(ImageDisplay &d)
{
    strip(d);
    title(d, "Zone Home", "3/14");
    struct Ch
    {
        std::string_view mode;
        std::string_view name;
        std::string_view freq;
    };
    const Ch rows[] = {
        {"D", "Local TG", "438.200"},
        {"D", "Wide TG", "438.200"},
        {"D", "Parrot", "438.200"},
        {"D", "City 2", "439.575"},
        {"F", "Calling", "145.500"},
        {"F", "City FM", "145.600"},
        {"F", "Simplex", "433.500"},
    };
    for (int i = 0; i < 7; ++i) {
        const int y = 24 + i * 13;
        const bool on = i == 2;
        const Color ground = on ? p::Text : p::Ground;
        d.fillRect(0, y, 156, 13, ground);
        text(d, Body, 4, y + 1, rows[i].mode, rows[i].mode == "D" ? p::Beacon : p::Receive, ground);
        text(d, Body, 16, y + 1, rows[i].name, on ? p::Ground : p::Text, ground);
        right(d, Body, 152, y + 1, rows[i].freq, on ? p::Ground : p::Dim, ground);
    }
    scrollBar(d, 24, 91, 0, 7, 14);
    hint(d, "MENU EDIT # NEW * MOVE");
}
void editor(ImageDisplay &d, std::string_view page, const Row *rows, int count, int chosen, bool changed)
{
    strip(d);
    d.fillRect(0, 11, 160, 13, p::Panel);
    text(d, Body, 4, 12, "Parrot", p::Text, p::Panel);
    if (changed) {
        text(d, Body, 4 + width(Body, "Parrot") + 2, 12, "*", p::Warn, p::Panel);
    }
    right(d, Body, 156, 12, page, p::Rule, p::Panel);
    for (int i = 0; i < count; ++i) {
        const int y = 24 + i * 13;
        const bool on = i == chosen;
        const Color ground = on ? p::Text : p::Ground;
        d.fillRect(0, y, 160, 13, ground);
        text(d, Body, 4, y + 1, rows[i].label, on ? p::Ground : p::Dim, ground);
        const bool edited = changed && i == 1;
        text(d, Body, 58, y + 1, rows[i].value, on ? p::Ground : edited ? p::Warn
                                                                        : p::Text,
            ground);
    }
}
void dBasic(ImageDisplay &d)
{
    const Row rows[] = {
        {"Name", "Parrot"},
        {"RX", "438.2125"},
        {"TX", "430.6125"},
        {"Mode", "DMR"},
        {"Power", "Low"},
        {"Band", "12.5 kHz"},
        {"RX only", "Off"},
    };
    editor(d, "1/3 Basic", rows, 7, 1, true);
    hint(d, "UP/DN PAGE MENU EDIT EXIT SAVE");
}
void dDmr(ImageDisplay &d)
{
    const Row rows[] = {
        {"CC", "1"},
        {"Slot", "TS2"},
        {"Contact", "Parrot"},
        {"Group", "Group 1"},
        {"Roaming", "On"},
        {"APRS", "Off"},
        {"Admit", "Free"},
    };
    editor(d, "2/3 DMR", rows, 7, 4, false);
    hint(d, "UP/DN PAGE MENU EDIT EXIT SAVE");
}
void dFrequency(ImageDisplay &d)
{
    strip(d);
    title(d, "Parrot > RX", "");
    text(d, Large, 8, 38, "438.212", p::Text, p::Ground);
    text(d, Large, 8 + 7 * 12, 38, "5", p::Rule, p::Ground);
    text(d, Body, 112, 46, "MHz", p::Dim, p::Ground);
    d.fillRect(8 + 7 * 12, 63, 12, 2, p::Warn);
    text(d, Body, 8, 76, "TX follows: -7.6 MHz", p::Dim, p::Ground);
    text(d, Body, 8, 90, "Knob: 12.5 kHz steps", p::Rule, p::Ground);
    hint(d, "0-9 DIGITS * DELETE MENU SET");
}
void dName(ImageDisplay &d)
{
    strip(d);
    title(d, "Parrot > Name", "6/16");
    text(d, Large, 8, 34, "Parro", p::Text, p::Ground);
    d.fillRect(8 + 5 * 12, 34, 12, 24, p::Warn);
    text(d, Large, 8 + 5 * 12, 34, "t", p::Ground, p::Warn);
    // The letters on the key pressed, the one taken lifted.
    const std::string_view keys[] = {"t", "u", "v", "8"};
    for (int i = 0; i < 4; ++i) {
        const int x = 24 + i * 28;
        const bool on = i == 0;
        d.fillRect(x, 70, 24, 20, on ? p::Text : p::Panel);
        centre(d, Medium, 72, keys[i], on ? p::Ground : p::Text, on ? p::Text : p::Panel, x, 24);
    }
    centre(d, Body, 96, "8 again: next letter", p::Rule, p::Ground);
    hint(d, "# CASE * DELETE KNOB MOVE");
}
void dSave(ImageDisplay &d)
{
    strip(d);
    title(d, "Save Parrot?", "2 changes");
    text(d, Body, 4, 28, "RX", p::Dim, p::Ground);
    text(d, Body, 28, 28, "438.2000", p::Rule, p::Ground);
    text(d, Body, 28, 41, "438.2125", p::Warn, p::Ground);
    text(d, Body, 4, 58, "TX", p::Dim, p::Ground);
    text(d, Body, 28, 58, "430.6000", p::Rule, p::Ground);
    text(d, Body, 28, 71, "430.6125", p::Warn, p::Ground);
    d.fillRect(0, 88, 160, 28, p::Panel);
    text(d, Body, 4, 90, "Written to the plug;", p::Dim, p::Panel);
    text(d, Body, 4, 102, "the CPS still reads it", p::Dim, p::Panel);
    hint(d, "MENU SAVE EXIT EDIT # DROP");
}

} // namespace

int main(int argc, char **argv)
{
    const QString out = argc > 1 ? QString::fromLocal8Bit(argv[1]) : QStringLiteral(".");
    QDir().mkpath(out);
    struct Screen
    {
        const char *name;
        void (*draw)(ImageDisplay &);
    };
    const Screen screens[] = {
        {"a1-top", aTop},
        {"a2-channel", aChannel},
        {"a3-editing", aEditing},
        {"b1-tiles", bTiles},
        {"b2-list", bList},
        {"c1-detail", cList},
        {"c2-number", cNumber},
        {"c3-choice", cChoice},
        {"d1-plug", dPlug},
        {"d2-channels", dChannels},
        {"d3-basic", dBasic},
        {"d4-dmr", dDmr},
        {"d5-frequency", dFrequency},
        {"d6-name", dName},
        {"d7-save", dSave},
    };
    for (const Screen &sc : screens) {
        ImageDisplay d;
        sc.draw(d);
        const QString file = QStringLiteral("%1/%2.png").arg(out, QString::fromLatin1(sc.name));
        d.save(file);
        std::printf("%s\n", file.toLocal8Bit().constData());
    }
    return 0;
}
