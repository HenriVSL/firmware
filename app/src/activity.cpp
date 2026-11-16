// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Henri Lavikainen

#include "activity.h"

#include "standby.h"

#include "dmm/lib/format.h"
#include "dmm/services/font.h"

namespace dmm::app::ui {

namespace {

using services::Color;
using services::Display;
using services::Font;
namespace fonts = services::fonts;
namespace p = palette;

const Font &Small = fonts::Spleen5x8;
const Font &Body = fonts::Spleen6x12;
const Font &Medium = fonts::Spleen8x16;
const Font &Large = fonts::Spleen12x24;

constexpr int StatusH = StandbyScreen::StatusH;
constexpr int HintH = StandbyScreen::HintH;
// The transmit screen's rows.
constexpr int StateY = StatusH;
constexpr int WhoY = StateY + 14;
constexpr int WhoNameY = WhoY + 26;
constexpr int TimeY = WhoNameY + 14;
constexpr int TimeEndY = TimeY + 26;
// The activity page's first row.
constexpr int LogTop = StatusH + 15;

int centred(const Display &d, const Font &f, std::string_view text)
{
    return (d.width() - textWidth(f, text)) / 2;
}

} // namespace

void drawTransmit(Display &d, const TransmitInfo &t)
{
    const int w = d.width();
    const int h = d.height();
    constexpr std::string_view State = "TRANSMITTING";
    band(d, StateY, WhoY - StateY, {{centred(d, Small, State), StateY + 4, State, &Small, p::Transmit}});
    const std::string_view target = cut(t.target, (w - 8) / Medium.width);
    const Font &tf = textWidth(Large, target) <= w - 8 ? Large : Medium;
    band(d, WhoY, WhoNameY - WhoY,
        {{centred(d, tf, target), WhoY + (Large.height - tf.height) / 2, target, &tf, p::Text}});
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

void drawTransmitTime(Display &d, const TransmitInfo &t)
{
    lib::StaticString<12> time;
    time << t.seconds / 60 << ':' << static_cast<char>('0' + t.seconds % 60 / 10)
         << static_cast<char>('0' + t.seconds % 10);
    band(d, TimeY, TimeEndY - TimeY, {{centred(d, Medium, time.view()), TimeY + 5, time.view(), &Medium, p::Text}});
}

void drawLogFrame(Display &d, std::string_view title, std::string_view note, std::string_view hint)
{
    const int w = d.width();
    const int h = d.height();
    band(d, StatusH, LogTop - StatusH,
        {{4, StatusH + 2, title, &Body, p::Text}, {w - 4 - textWidth(Small, note), StatusH + 5, note, &Small, p::Dim}});
    band(d, LogTop + LogRows * LogRowH, h - HintH - LogTop - LogRows * LogRowH, {});
    d.fillRect(0, h - HintH, w, 1, p::Rule);
    band(d, h - HintH + 1, HintH - 1, {{3, h - HintH + 2, hint, &Small, p::Dim}});
}

void drawLogRow(Display &d, int row, std::string_view time, std::string_view text, Color ink, bool selected)
{
    const int y = LogTop + row * LogRowH;
    band(d, y, LogRowH, {{4, y + 1, time, &Small, p::Dim}, {34, y + 1, cut(text, 25), &Small, ink}},
        selected ? p::Panel : p::Ground);
}

} // namespace dmm::app::ui
