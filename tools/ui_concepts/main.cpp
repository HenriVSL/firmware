// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Henri Lavikainen

// Draws standby screen concepts with the radio's own fonts and text drawing
// at the panel's 160 x 128 and saves them as PNG files, scaled up.
// usage: ui-concepts <output folder>

#include "dmm/services/display.h"
#include "dmm/services/font.h"

#include <QDir>
#include <QImage>
#include <QString>

#include <cstdio>
#include <string>
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

int width(const Font &f, std::string_view s)
{
    return static_cast<int>(s.size()) * f.width;
}

// Text helpers: left, right-aligned and centred.
void text(ImageDisplay &d, const Font &f, int x, int y, std::string_view s, Color fg, Color bg)
{
    dmm::services::drawText(d, f, x, y, s, fg, bg);
}
void right(ImageDisplay &d, const Font &f, int xr, int y, std::string_view s, Color fg, Color bg)
{
    text(d, f, xr - width(f, s), y, s, fg, bg);
}
void centre(ImageDisplay &d, const Font &f, int y, std::string_view s, Color fg, Color bg)
{
    text(d, f, (160 - width(f, s)) / 2, y, s, fg, bg);
}
void hline(ImageDisplay &d, int x, int y, int w, Color c)
{
    d.fillRect(x, y, w, 1, c);
}
void vline(ImageDisplay &d, int x, int y, int h, Color c)
{
    d.fillRect(x, y, 1, h, c);
}
void box(ImageDisplay &d, int x, int y, int w, int h, Color c)
{
    hline(d, x, y, w, c);
    hline(d, x, y + h - 1, w, c);
    vline(d, x, y, h, c);
    vline(d, x + w - 1, y, h, c);
}
// The palette in use today.
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
} // namespace p

enum class State
{
    Idle,
    Call,
};

// --- A: Clean. Time and battery voltage only; state shows as a thin
// coloured line and one word, only while it happens. ---
void clean(ImageDisplay &d, State s)
{
    d.fillRect(0, 0, 160, 128, p::Ground);
    text(d, Body, 4, 3, "14:32", p::Dim, p::Ground);
    right(d, Body, 156, 3, "7.9V", p::Dim, p::Ground);
    if (s == State::Call) {
        d.fillRect(0, 17, 160, 2, p::Receive);
        centre(d, Small, 6, "RX", p::Receive, p::Ground);
    } else {
        hline(d, 0, 18, 160, p::Rule);
    }
    if (s == State::Idle) {
        centre(d, Small, 32, "ZONE  DMR", p::Dim, p::Ground);
        centre(d, Large, 46, "TG 244", p::Text, p::Ground);
        centre(d, Body, 74, "Finland  TS1", p::Dim, p::Ground);
        centre(d, Small, 92, "via repeater 2", p::Rule, p::Ground);
    } else {
        centre(d, Small, 30, "GROUP CALL  TG 244", p::Receive, p::Ground);
        centre(d, Large, 44, "N0CALL", p::Text, p::Ground);
        centre(d, Body, 72, "Test Operator", p::Dim, p::Ground);
        centre(d, Small, 92, "0:07", p::Dim, p::Ground);
    }
    // Only what is not as it should be shows here.
    if (s == State::Idle) {
        text(d, Small, 4, 117, "GPS no fix", p::Warn, p::Ground);
    }
}

// --- B: Dashboard. Every number on one screen, in labelled cells. ---
void cell(ImageDisplay &d, int x, int y, int w, std::string_view label, std::string_view value, Color ink)
{
    d.fillRect(x, y, w, 20, p::Panel);
    text(d, Small, x + 3, y + 2, label, p::Dim, p::Panel);
    text(d, Body, x + 3, y + 9, value, ink, p::Panel);
}
void dashboard(ImageDisplay &d, State s)
{
    d.fillRect(0, 0, 160, 128, p::Ground);
    // Top bar: name of the channel, time, voltage.
    text(d, Body, 3, 2, s == State::Call ? "RX TG 244" : "TG 244 Finland", s == State::Call ? p::Receive : p::Text,
        p::Ground);
    right(d, Small, 157, 4, "7.9V 14:32", p::Dim, p::Ground);
    hline(d, 0, 15, 160, p::Rule);
    cell(d, 2, 18, 77, "RX MHz", "438.2125", p::Text);
    cell(d, 81, 18, 77, "TX MHz", "430.6125", p::Dim);
    cell(d, 2, 40, 77, "SLOT  CC", "TS1   CC1", p::Text);
    cell(d, 81, 40, 77, "REPEATER", "2  -78dBm", p::Receive);
    cell(d, 2, 62, 77, "GPS", "3D  9 sat", p::Receive);
    cell(d, 81, 62, 77, "APRS", "next 2:10", p::Beacon);
    cell(d, 2, 84, 77, "POWER", "LOW", p::Text);
    cell(d, 81, 84, 77, "VOLUME", "8", p::Text);
    hline(d, 0, 107, 160, p::Rule);
    if (s == State::Call) {
        text(d, Small, 3, 110, "N0CALL  Test Operator", p::Receive, p::Ground);
        text(d, Small, 3, 119, "group TG 244   0:07", p::Dim, p::Ground);
    } else {
        text(d, Small, 3, 110, "last  N0CALL  3 min ago", p::Dim, p::Ground);
        text(d, Small, 3, 119, "roam  2 Good  beacon 60s", p::Dim, p::Ground);
    }
}

// --- C: Activity. The channel on top, below it what happened, newest
// first, so a glance shows what the radio has been doing. ---
void activity(ImageDisplay &d, State s)
{
    d.fillRect(0, 0, 160, 128, p::Ground);
    d.fillRect(0, 0, 160, 30, p::Panel);
    text(d, Medium, 4, 2, "TG 244 Finland", p::Text, p::Panel);
    text(d, Small, 4, 20, "438.2125  TS1 CC1  REP 2", p::Dim, p::Panel);
    right(d, Small, 156, 20, "7.9V", p::Dim, p::Panel);
    struct Row
    {
        const char *time;
        const char *what;
        Color ink;
    };
    const Row idle[] = {
        {"14:31", "RX N0CALL > TG244  12s", p::Receive},
        {"14:30", "ROAM rep 2 good -78", p::Text},
        {"14:28", "APRS beacon sent", p::Beacon},
        {"14:25", "TX TG244  4s", p::Transmit},
        {"14:22", "GPS fix  9 sat", p::Dim},
        {"14:20", "ROAM rep 1 lost", p::Warn},
        {"14:20", "ROAM search > rep 2", p::Dim},
    };
    const Row call[] = {
        {"NOW", "RX N0CALL > TG244   7s", p::Receive},
        {"14:31", "RX N0CALL > TG244  12s", p::Dim},
        {"14:30", "ROAM rep 2 good -78", p::Dim},
        {"14:28", "APRS beacon sent", p::Dim},
        {"14:25", "TX TG244  4s", p::Dim},
        {"14:22", "GPS fix  9 sat", p::Dim},
        {"14:20", "ROAM rep 1 lost", p::Dim},
    };
    const Row *rows = s == State::Call ? call : idle;
    for (int i = 0; i < 7; ++i) {
        const int y = 33 + i * 11;
        if (s == State::Call && i == 0) {
            d.fillRect(0, y - 1, 160, 11, rgb(20, 60, 25));
            text(d, Small, 4, y + 1, rows[i].time, p::Receive, rgb(20, 60, 25));
            text(d, Small, 34, y + 1, rows[i].what, p::Text, rgb(20, 60, 25));
            continue;
        }
        text(d, Small, 4, y + 1, rows[i].time, p::Rule, p::Ground);
        text(d, Small, 34, y + 1, rows[i].what, rows[i].ink, p::Ground);
    }
    hline(d, 0, 112, 160, p::Rule);
    text(d, Small, 4, 117, "14:32", p::Dim, p::Ground);
    right(d, Small, 156, 117, "GPS 9  APRS 2:10  VOL 8", p::Dim, p::Ground);
}

// --- D: Utility. One colour, capitals, fixed fields in boxes, nothing
// moves; a field changes only its value. ---
void utility(ImageDisplay &d, State s)
{
    const Color ink = rgb(255, 176, 0);
    const Color dim = rgb(140, 95, 0);
    const Color g = p::Ground;
    d.fillRect(0, 0, 160, 128, g);
    box(d, 0, 0, 160, 128, dim);
    // Status line: mode, then time in UTC.
    if (s == State::Call) {
        d.fillRect(2, 1, 16, 11, ink);
        text(d, Small, 5, 3, "RX", g, ink);
    } else {
        text(d, Small, 4, 3, "RDY", ink, g);
    }
    centre(d, Small, 3, "CH 01  ZONE A", dim, g);
    right(d, Small, 156, 3, "1132Z", ink, g);
    hline(d, 0, 12, 160, dim);
    // The channel, large.
    text(d, Small, 4, 16, "NET", dim, g);
    text(d, Medium, 28, 15, "TG244 FINLAND", ink, g);
    hline(d, 0, 33, 160, dim);
    // Two columns of fixed fields.
    auto field = [&](int x, int y, std::string_view label, std::string_view value) {
        text(d, Small, x, y, label, dim, g);
        text(d, Small, x + 30, y, value, ink, g);
    };
    field(4, 37, "FREQ", "438.2125");
    field(84, 37, "TS/CC", "1/1");
    field(4, 47, "PWR", "LO");
    field(84, 47, "SITE", "REP 2");
    field(4, 57, "BATT", "7.9V");
    field(84, 57, "SIG", "-78DBM");
    hline(d, 0, 67, 160, dim);
    field(4, 71, "POS", "60 10.0N 024 56.0E");
    field(4, 81, "FIX", "3D  9 SAT  HDOP 1.1");
    hline(d, 0, 91, 160, dim);
    if (s == State::Call) {
        field(4, 95, "FROM", "N0CALL  2440001");
        field(4, 105, "TO", "TG244   T+0007");
    } else {
        field(4, 95, "LAST", "N0CALL  T-0300");
        field(4, 105, "APRS", "AUTO OFF  NEXT --");
    }
    hline(d, 0, 115, 160, dim);
    text(d, Small, 4, 118, "MENU:OPTS  P1:ZONE  P2:CH", dim, g);
}

// --- Second round: one working screen, call screens in the clean style,
// the activity list behind a key, and a screen saver. ---

// What the status strip shows: time (local or UTC) and battery on the
// ends; in between only marks that matter now.
struct Strip
{
    const char *time = "14:32";
    bool aprs = false;      // beacons go out by themselves
    bool gpsSearch = false; // on, but no fix yet
};

void strip(ImageDisplay &d, const Strip &st)
{
    text(d, Small, 4, 2, st.time, p::Dim, p::Ground);
    right(d, Small, 156, 2, "7.9V", p::Dim, p::Ground);
    int x = 64;
    if (st.gpsSearch) {
        text(d, Small, x, 2, "GPS..", p::Warn, p::Ground);
        x += 32;
    }
    if (st.aprs) {
        text(d, Small, x, 2, "APRS", p::Beacon, p::Ground);
    }
    hline(d, 0, 11, 160, p::Rule);
}

void label(ImageDisplay &d, int x, int y, std::string_view l, std::string_view v, Color ink, Color bg)
{
    text(d, Small, x, y + 3, l, p::Dim, bg);
    text(d, Body, x + width(Small, l) + 4, y, v, ink, bg);
}

// The working screen.
void home(ImageDisplay &d, const Strip &st)
{
    d.fillRect(0, 0, 160, 128, p::Ground);
    strip(d, st);
    // The talkgroup, as large as fits, its name beside it.
    text(d, Small, 4, 16, "TALKGROUP", p::Dim, p::Ground);
    right(d, Small, 156, 16, "DMR  3/13", p::Rule, p::Ground);
    text(d, Large, 4, 27, "TG 244", p::Text, p::Ground);
    text(d, Body, 84, 38, "Finland", p::Dim, p::Ground);
    hline(d, 0, 55, 160, p::Rule);
    // Frequency and slot.
    d.fillRect(0, 56, 160, 22, p::Panel);
    label(d, 4, 61, "MHz", "438.2125", p::Text, p::Panel);
    text(d, Body, 102, 61, "TS1", p::Text, p::Panel);
    text(d, Small, 126, 64, "CC1", p::Dim, p::Panel);
    // Power and volume on one line.
    label(d, 4, 84, "PWR", "LOW", p::Text, p::Ground);
    label(d, 102, 84, "VOL", "8", p::Text, p::Ground);
    hline(d, 0, 99, 160, p::Rule);
    text(d, Small, 4, 104, "via repeater 2   -78 dBm", p::Dim, p::Ground);
    text(d, Small, 4, 118, "MENU   P1 ZONE   P2 LOG", p::Rule, p::Ground);
}

// A call coming in: who, how long; green only where it says so.
void receiving(ImageDisplay &d, const Strip &st)
{
    d.fillRect(0, 0, 160, 128, p::Ground);
    strip(d, st);
    d.fillRect(0, 12, 160, 2, p::Receive);
    centre(d, Small, 18, "RECEIVING  TG 244", p::Receive, p::Ground);
    centre(d, Large, 30, "N0CALL", p::Text, p::Ground);
    centre(d, Body, 57, "Test Operator", p::Dim, p::Ground);
    centre(d, Small, 72, "ID 2440001", p::Rule, p::Ground);
    centre(d, Medium, 86, "0:07", p::Text, p::Ground);
    hline(d, 0, 112, 160, p::Rule);
    text(d, Small, 4, 117, "TS1   438.2125", p::Dim, p::Ground);
}

// On the air: which talkgroup, how long; red only where it says so.
void transmitting(ImageDisplay &d, const Strip &st)
{
    d.fillRect(0, 0, 160, 128, p::Ground);
    strip(d, st);
    d.fillRect(0, 12, 160, 2, p::Transmit);
    centre(d, Small, 18, "TRANSMITTING", p::Transmit, p::Ground);
    centre(d, Large, 32, "TG 244", p::Text, p::Ground);
    centre(d, Body, 59, "Finland", p::Dim, p::Ground);
    centre(d, Medium, 82, "0:04", p::Text, p::Ground);
    hline(d, 0, 112, 160, p::Rule);
    text(d, Small, 4, 117, "TS1   POWER LOW", p::Dim, p::Ground);
}

// The activity list, opened with a key.
void log(ImageDisplay &d, const Strip &st)
{
    d.fillRect(0, 0, 160, 128, p::Ground);
    strip(d, st);
    text(d, Body, 4, 15, "Activity", p::Text, p::Ground);
    right(d, Small, 156, 18, "newest first", p::Rule, p::Ground);
    struct Row
    {
        const char *time;
        const char *what;
        Color ink;
    };
    const Row rows[] = {
        {"14:31", "RX N0CALL  TG244  12s", p::Receive},
        {"14:30", "ROAM rep 2  -78", p::Text},
        {"14:28", "APRS beacon sent", p::Beacon},
        {"14:25", "TX TG244  4s", p::Transmit},
        {"14:22", "GPS fix", p::Dim},
        {"14:20", "ROAM rep 1 lost", p::Warn},
        {"14:18", "RX N1TEST  TG91  31s", p::Receive},
        {"14:12", "TX TG91  6s", p::Transmit},
    };
    for (int i = 0; i < 8; ++i) {
        const int y = 30 + i * 11;
        text(d, Small, 4, y, rows[i].time, p::Rule, p::Ground);
        text(d, Small, 34, y, rows[i].what, rows[i].ink, p::Ground);
    }
    hline(d, 0, 118, 160, p::Rule);
    text(d, Small, 4, 120, "KNOB SCROLL   P2 BACK", p::Rule, p::Ground);
}

// After a while idle: big time, the talkgroup, the battery, dim.
void saver(ImageDisplay &d, const Strip &)
{
    d.fillRect(0, 0, 160, 128, p::Ground);
    centre(d, Large, 34, "14:32", p::Dim, p::Ground);
    centre(d, Small, 62, "1132Z", p::Rule, p::Ground);
    centre(d, Body, 82, "TG 244 Finland", p::Dim, p::Ground);
    centre(d, Small, 112, "7.9V", p::Rule, p::Ground);
}

} // namespace

int main(int argc, char **argv)
{
    const QString out = argc > 1 ? QString::fromLocal8Bit(argv[1]) : QStringLiteral(".");
    QDir().mkpath(out);
    struct Concept
    {
        const char *name;
        void (*draw)(ImageDisplay &, State);
    };
    const Concept concepts[] = {
        {"a-clean", clean},
        {"b-dashboard", dashboard},
        {"c-activity", activity},
        {"d-utility", utility},
    };
    for (const Concept &c : concepts) {
        for (const State s : {State::Idle, State::Call}) {
            ImageDisplay d;
            c.draw(d, s);
            const QString file = QStringLiteral("%1/%2-%3.png").arg(out, QString::fromLatin1(c.name), s == State::Idle ? QStringLiteral("idle") : QStringLiteral("call"));
            d.save(file);
            std::printf("%s\n", file.toLocal8Bit().constData());
        }
    }
    struct Screen
    {
        const char *name;
        void (*draw)(ImageDisplay &, const Strip &);
        Strip strip;
    };
    const Screen screens[] = {
        {"e-home", home, {}},
        {"e-home-marks", home, {"1132Z", true, true}},
        {"f-receiving", receiving, {}},
        {"g-transmitting", transmitting, {}},
        {"h-log", log, {}},
        {"i-saver", saver, {}},
    };
    for (const Screen &sc : screens) {
        ImageDisplay d;
        sc.draw(d, sc.strip);
        const QString file = QStringLiteral("%1/%2.png").arg(out, QString::fromLatin1(sc.name));
        d.save(file);
        std::printf("%s\n", file.toLocal8Bit().constData());
    }
    return 0;
}
