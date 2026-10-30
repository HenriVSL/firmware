// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tommi Tauriainen

#include "ui.h"

#include "aprs.h"
#include "audio.h"
#include "gps.h"
#include "radio.h"
#include "standby.h"
#include "system.h"

#include "dmm/lib/variant.h"
#include "dmm/os/os.h"
#include "dmm/services/channel_selector.h"
#include "dmm/services/clock.h"
#include "dmm/services/codeplug.h"
#include "dmm/services/event_log.h"
#include "dmm/services/input.h"
#include "dmm/services/log.h"
#include "dmm/services/power.h"
#include "dmm/version.h"

#include <algorithm>
#include <atomic>
#include <cstring>
#include <iterator>
#include <string_view>

namespace dmm::app::ui {

namespace {

using namespace services;
using services::log::Level;
namespace log = services::log;

os::Task<1024> uiTask;
os::Queue<Message, 16> inbox;
// The codeplug editor, and a console command waiting for it.
board::PlugEditor *plugEditor = nullptr;
lib::StaticString<128> plugCommandText;
std::atomic<bool> plugCommandWaiting{false};
Display *display = nullptr;
CodeplugSource *codeplug = nullptr;
const board::Input *input = nullptr;

// The label on @p key as the user sees it.
std::string_view keyText(Key key)
{
    return input != nullptr ? input->keyLabel(key) : keyName(key);
}

// The header band's colour: black on the development views, the status
// bar's grey on the radio's own screens.
Color headerGround = colors::Black;

constexpr int DefaultBacklight = 4;
// How long the start screen shows the name and version.
constexpr os::Ticks StartScreenMs = 1200;
constexpr os::Ticks RedrawStepMs = 100;

// The log view: a header in the small font, then rows of wrapped log text.
const Font &HeaderFont = fonts::Spleen5x8;
const Font &LogFont = fonts::Spleen6x12;
constexpr int MaxColumns = 32;
constexpr int MaxRows = 12;

struct Row
{
    char text[MaxColumns];
    std::uint8_t length;
    Level level;
};

class LogView
{
public:
    void configure(int columns, int rows)
    {
        m_columns = columns < MaxColumns ? columns : MaxColumns;
        m_rows = rows < MaxRows ? rows : MaxRows;
    }

    // Called from any task that logs (log tap).
    void add(Level level, std::string_view line)
    {
        // Drop the time stamp: the header shows the uptime.
        const std::size_t close = line.find("] ");
        if (line.starts_with('[') && close != std::string_view::npos) {
            line.remove_prefix(close + 2);
        }
        m_lock.lock();
        do {
            const std::size_t n = line.size() < static_cast<std::size_t>(m_columns) ? line.size() : static_cast<std::size_t>(m_columns);
            Row &row = m_ring[m_next];
            for (std::size_t i = 0; i < n; ++i) {
                row.text[i] = line[i];
            }
            row.length = static_cast<std::uint8_t>(n);
            row.level = level;
            m_next = (m_next + 1) % MaxRows;
            if (m_count < MaxRows) {
                ++m_count;
            }
            line.remove_prefix(n);
        } while (!line.empty());
        m_dirty = true;
        m_lock.unlock();
    }

    // Draws the rows if anything changed; the newest at the bottom.
    void draw(Display &d, int top, bool force)
    {
        Row rows[MaxRows];
        int count = 0;
        m_lock.lock();
        const bool dirty = m_dirty || force;
        m_dirty = false;
        if (dirty) {
            count = m_count < m_rows ? m_count : m_rows;
            for (int i = 0; i < count; ++i) {
                rows[i] = m_ring[(m_next + MaxRows - count + i) % MaxRows];
            }
        }
        m_lock.unlock();
        if (!dirty) {
            return;
        }
        const int first = m_rows - count;
        for (int r = 0; r < m_rows; ++r) {
            const int y = top + r * LogFont.height;
            int x = 0;
            if (r >= first) {
                const Row &row = rows[r - first];
                x = drawText(d, LogFont, 0, y, std::string_view(row.text, row.length), colorFor(row.level), colors::Black);
            }
            d.fillRect(x, y, d.width() - x, LogFont.height, colors::Black);
        }
    }

private:
    static Color colorFor(Level level)
    {
        switch (level) {
        case Level::Debug:
            return colors::Grey;
        case Level::Warning:
            return colors::Yellow;
        case Level::Error:
            return rgb(255, 80, 80);
        case Level::Info:
            break;
        }
        return colors::White;
    }

    os::Mutex m_lock;
    Row m_ring[MaxRows]{};
    int m_next = 0;
    int m_count = 0;
    int m_columns = 26;
    int m_rows = 10;
    bool m_dirty = false;
};

LogView logView;

void onLogLine(Level level, std::string_view line)
{
    logView.add(level, line);
}

// The last battery state from the system task; millivolts 0 = unknown.
struct BatteryState
{
    int millivolts = 0;
    int level = 0;
    bool low = false;
};

BatteryState batteryState;

// Right-aligned in the header: four bars (red when low) and the voltage.
// The radio's own screens put it in their taller status bar.
int headerHeight = 8;
int headerTop = 0;

void fillArea(Display &d, int x, int y, int w, int h, Color c)
{
    if (w > 0 && h > 0) {
        d.fillRect(x, y, w, h, c);
    }
}

// A battery at the header's right end, filled by its level from the left
// so the outline shows as it drains, red when low; in its own field from 48
// pixels before the edge. Every pixel is painted once, so an update does
// not flicker.
void drawBattery(Display &d)
{
    const BatteryState &b = batteryState;
    const int w = d.width();
    const int fieldX = w - 48;
    constexpr int BodyW = 21;
    constexpr int BodyH = 9;
    constexpr int InnerW = BodyW - 2;
    const int x = w - 5 - BodyW;
    const int y = headerTop == 0 ? 0 : 1;
    const Color ink = b.low ? colors::Red : palette::Text;
    int filled = 0;
    if (b.millivolts > 0) {
        // A low battery keeps a sliver.
        filled = std::max(b.level * InnerW / 3, b.low ? 3 : 0);
    }
    fillArea(d, fieldX, 0, x - fieldX, headerHeight, headerGround);
    fillArea(d, x, 0, w - x, y, headerGround);
    fillArea(d, x, y + BodyH, w - x, headerHeight - y - BodyH, headerGround);
    d.fillRect(x, y, BodyW, 1, ink);
    d.fillRect(x, y + BodyH - 1, BodyW, 1, ink);
    d.fillRect(x, y + 1, 1, BodyH - 2, ink);
    d.fillRect(x + BodyW - 1, y + 1, 1, BodyH - 2, ink);
    fillArea(d, x + 1, y + 1, filled, BodyH - 2, ink);
    fillArea(d, x + 1 + filled, y + 1, InnerW - filled, BodyH - 2, headerGround);
    d.fillRect(x + BodyW, y + 2, 2, BodyH - 4, ink);
    fillArea(d, x + BodyW, y, 2, 2, headerGround);
    fillArea(d, x + BodyW, y + BodyH - 2, 2, 2, headerGround);
    fillArea(d, x + BodyW + 2, y, w - x - BodyW - 2, BodyH, headerGround);
}

services::KeyLock keyLock;
bool transmitting = false;

// The header's lock mark, left of the battery: a yellow LOCK box while the
// keys are locked, blank otherwise. Header text must end before LockMarkX.
constexpr int LockMarkW = 4 * 5 + 3;
int lockMarkX(const Display &d)
{
    return d.width() - 48 - LockMarkW;
}

void drawLockMark(Display &d)
{
    const int x = lockMarkX(d);
    if (keyLock.locked() || keyLock.settings().forced) {
        d.fillRect(x, 0, LockMarkW, HeaderFont.height, colors::Yellow);
        drawText(d, HeaderFont, x + 1, 0, "LOCK", colors::Black, colors::Yellow);
    } else {
        d.fillRect(x, 0, LockMarkW, HeaderFont.height, colors::Black);
    }
}

void drawHeader(Display &d)
{
    lib::StaticString<40> text;
    text << Version << " up " << (os::nowMs() / 1000) << "s";
    const int x = drawText(d, HeaderFont, 0, 0, text.view(), colors::Grey, colors::Black);
    d.fillRect(x, 0, lockMarkX(d) - x, HeaderFont.height, colors::Black);
    drawLockMark(d);
    drawBattery(d);
}

// A short notice in a band across the middle of the screen, drawn over
// what is there without blanking it first.
void drawNotice(Display &d, std::string_view text)
{
    const Font &font = fonts::Spleen8x16;
    const int h = font.height + 10;
    const int y = (d.height() - h) / 2;
    const int w = d.width();
    const int textW = static_cast<int>(text.size()) * font.width;
    const int tx = (w - textW) / 2;
    const int ty = y + 5;
    drawText(d, font, tx, ty, text, palette::Text, palette::Ground);
    d.fillRect(0, y, w, 1, palette::Rule);
    fillArea(d, 0, y + 1, w, ty - y - 1, palette::Ground);
    fillArea(d, 0, ty, tx, font.height, palette::Ground);
    fillArea(d, tx + textW, ty, w - tx - textW, font.height, palette::Ground);
    fillArea(d, 0, ty + font.height, w, y + h - 1 - ty - font.height, palette::Ground);
    d.fillRect(0, y + h - 1, w, 1, palette::Rule);
}

// The volume while the knob turns: "Volume" and a 15-segment bar. With
// @p frame the whole band is drawn over what is there; without, only the
// number and the segments change, in place.
void drawVolumeNotice(Display &d, int step, bool frame)
{
    const Font &font = fonts::Spleen6x12;
    const int h = font.height + 16;
    const int y = (d.height() - h) / 2;
    const int w = d.width();
    const int labelY = y + 3;
    const int segY = y + font.height + 6;
    constexpr int SegH = 5;
    constexpr int LabelChars = 9; // "Volume 15"
    lib::StaticString<12> label;
    label << "Volume " << step;
    while (label.view().size() < LabelChars) {
        label << ' ';
    }
    drawText(d, font, 6, labelY, label.view(), palette::Text, palette::Ground);
    constexpr int Segments = 15;
    const int segW = (w - 12) / Segments;
    for (int i = 0; i < Segments; ++i) {
        d.fillRect(6 + i * segW, segY, segW - 2, SegH, i < step ? palette::Text : palette::Rule);
    }
    if (!frame) {
        return;
    }
    const int labelEnd = 6 + LabelChars * font.width;
    d.fillRect(0, y, w, 1, palette::Rule);
    fillArea(d, 0, y + 1, w, labelY - y - 1, palette::Ground);
    fillArea(d, 0, labelY, 6, font.height, palette::Ground);
    fillArea(d, labelEnd, labelY, w - labelEnd, font.height, palette::Ground);
    fillArea(d, 0, labelY + font.height, w, segY - labelY - font.height, palette::Ground);
    fillArea(d, 0, segY, 6, SegH, palette::Ground);
    for (int i = 0; i < Segments; ++i) {
        fillArea(d, 6 + i * segW + segW - 2, segY, 2, SegH, palette::Ground);
    }
    fillArea(d, 6 + Segments * segW, segY, w - 6 - Segments * segW, SegH, palette::Ground);
    fillArea(d, 0, segY + SegH, w, y + h - 1 - segY - SegH, palette::Ground);
    d.fillRect(0, y + h - 1, w, 1, palette::Rule);
}

void logLockSettings()
{
    const services::KeyLockSettings &s = keyLock.settings();
    log::info("ui") << "key lock " << (keyLock.locked() ? "on" : "off") << "; covers" << (s.knob ? " knob" : "")
                    << (s.keypad ? " keypad" : "") << (s.side ? " side-keys+PTT" : "") << (s.forced ? ", forced" : "")
                    << ", auto " << s.autoLockMs / 1000 << " s";
}

// The start screen: the name and version on black.
// Under the version while the start screen waits for the radio.
void drawStarting(Display &d)
{
    const Font &small = fonts::Spleen6x12;
    constexpr std::string_view Text = "Please wait...";
    const int w = static_cast<int>(Text.size()) * small.width;
    drawText(d, small, (d.width() - w) / 2, d.height() - small.height - 8, Text, colors::Grey, colors::Black);
}

void drawStartScreen(Display &d)
{
    d.fillRect(0, 0, d.width(), d.height(), colors::Black);
    const Font &big = fonts::Spleen12x24;
    const Font &small = fonts::Spleen6x12;
    constexpr std::string_view Name = "DeltaMM";
    const int nameW = static_cast<int>(Name.size()) * big.width;
    const int y = (d.height() - big.height - small.height - 6) / 2;
    drawText(d, big, (d.width() - nameW) / 2, y, Name, colors::White, colors::Black);
    const std::string_view version(Version);
    const int versionW = static_cast<int>(version.size()) * small.width;
    drawText(d, small, (d.width() - versionW) / 2, y + big.height + 6, version, colors::Grey, colors::Black);
}

// Colour bars, a white frame, corner marks and sample text in each font,
// so position, orientation, colour order and glyphs can be checked by eye.
void drawTestPattern(Display &d)
{
    const Color bars[] = {colors::White, colors::Yellow, colors::Cyan, colors::Green, colors::Magenta, colors::Red,
        colors::Blue, colors::Black};
    const int w = d.width();
    const int h = d.height();
    for (int i = 0; i < 8; ++i) {
        d.fillRect(i * w / 8, 0, (i + 1) * w / 8 - i * w / 8, h, bars[i]);
    }
    d.fillRect(0, 0, w, 1, colors::White);
    d.fillRect(0, h - 1, w, 1, colors::White);
    d.fillRect(0, 0, 1, h, colors::White);
    d.fillRect(w - 1, 0, 1, h, colors::White);
    const Font &small = fonts::Spleen5x8;
    drawText(d, small, 2, 2, "TL", colors::Black, colors::White);
    drawText(d, small, w - 12, 2, "TR", colors::White, colors::Black);
    drawText(d, small, 2, h - 10, "BL", colors::Black, colors::White);
    drawText(d, small, w - 12, h - 10, "BR", colors::White, colors::Black);
    drawText(d, fonts::Spleen12x24, 2 + 12 * 2, 24, "DeltaMM", colors::Black, colors::White);
    drawText(d, fonts::Spleen8x16, 8, 56, "8x16 äöå ÄÖÅ", colors::White, colors::Black);
    drawText(d, fonts::Spleen6x12, 8, 76, "6x12 Hyvää päivää!", colors::White, colors::Black);
    drawText(d, fonts::Spleen5x8, 8, 92, "5x8 The quick brown fox", colors::White, colors::Black);
}

enum class View : std::uint8_t
{
    Start, // name and version while the radio starts
    TestPattern,
    Standby,    // the radio's main screen
    ZonePicker, // choosing a zone
    RoamStatus, // what roaming knows, refreshed each second
    Menu,       // the settings list
    MenuValue,  // choosing one setting's value
    Gps,        // the GPS receiver's state and fix, from the menu
    Heard,      // APRS stations heard, from the menu
    Station,    // one heard station
    Activity,   // what happened lately, from P2
    PlugList,   // a codeplug book's records, or a record to link to
    PlugEdit,   // one record's fields
    PlugValue,  // one field's value, chosen
    PlugDigits, // a frequency or a number, typed
    PlugText,   // a name or a text, typed
    PlugSave,   // what changed, before saving
    Log,
    KeyTest,
    Message,
};

// The radio's working channel, from the codeplug; nullptr without one.
ChannelSelector *channels = nullptr;
StandbyScreen standby;
ListPicker zonePicker;
ListPicker menuValue;
// The heard list's callsigns as last shown, the one chosen or open on its
// screen, and the first row on the screen.
char heardCalls[aprs::MaxHeard][aprs::HeardStation::CallBytes + 1] = {};
std::size_t heardShownCount = 0;
std::size_t stationShown = 0;
std::size_t heardTop = 0;

// The screens that show a list; the roaming status screen borrows the
// zone list's.
bool isList(View v)
{
    return v == View::ZonePicker || v == View::RoamStatus || v == View::Menu || v == View::MenuValue ||
        (v >= View::PlugList && v <= View::PlugSave);
}
ListPicker &listOf(View v)
{
    return v == View::MenuValue ? menuValue : zonePicker;
}

// The settings menu: one flat list, each item a list of values.
enum class MenuItem : std::uint8_t
{
    Squelch,
    SquelchMode,
    Power,
    Roaming,
    RoamZone,
    FrequencyOffset,
    RoamStatus, // opens the roaming status screen
    Backlight,
    BacklightTime,
    BigName,     // the channel's name or the contact's, large
    BesidePower, // the battery voltage or the volume next to the power
    BatterySave,
    KeyLock,
    AutoLock,
    Gps,
    GpsSystems,
    GpsStatus, // opens the GPS screen
    ClockZone,
    ClockShown, // local time or UTC on the screen
    ClockCalibration,
    AprsSend, // queues a beacon
    AprsBeacon,
    AprsPower,
    AprsReceive,
    PlugChannels, // the codeplug's books, each opens its list
    PlugZones,
    PlugContacts,
    PlugGroups,
    PlugRepeaters,
    PlugRoamZones,
    PlugRadioIds,
    PlugSettings,
    PlugMoreSettings,
    PlugAprs,
    PlugAprsText,
    ToolLog,  // opens the log
    ToolKeys, // opens the key test
    Version,
    Count,
};
MenuItem menuItem = MenuItem::Squelch;

// Menu groups and their items, by what the user is doing.
enum class MenuGroup : std::uint8_t
{
    Channel,
    Roaming,
    Position,
    Radio,
    Codeplug,
    Tools,
    Count,
};
struct GroupInfo
{
    const char *name;
    // Two lines about the group for the menu's panel.
    const char *about1;
    const char *about2;
    MenuItem items[11];
    std::uint8_t count;
};
constexpr GroupInfo Groups[] = {
    {"Channel", "Squelch, power and offset", "of the channel in use",
        {MenuItem::Squelch, MenuItem::SquelchMode, MenuItem::Power, MenuItem::FrequencyOffset}, 4},
    {"Roaming", "Moving between repeaters", "of the roaming zone",
        {MenuItem::Roaming, MenuItem::RoamZone, MenuItem::RoamStatus}, 3},
    {"Position", "The GPS receiver and", "APRS position beacons",
        {MenuItem::Gps, MenuItem::GpsSystems, MenuItem::GpsStatus, MenuItem::AprsBeacon, MenuItem::AprsPower,
            MenuItem::AprsReceive, MenuItem::AprsSend},
        7},
    {"Radio", "Display, keys, battery", "and the clock",
        {MenuItem::Backlight, MenuItem::BacklightTime, MenuItem::BigName, MenuItem::BesidePower, MenuItem::KeyLock,
            MenuItem::AutoLock, MenuItem::BatterySave, MenuItem::ClockZone, MenuItem::ClockShown,
            MenuItem::ClockCalibration},
        10},
    {"Codeplug", "Every record and value", "of the codeplug, edited",
        {MenuItem::PlugChannels, MenuItem::PlugZones, MenuItem::PlugContacts, MenuItem::PlugGroups,
            MenuItem::PlugRepeaters, MenuItem::PlugRoamZones, MenuItem::PlugRadioIds, MenuItem::PlugSettings,
            MenuItem::PlugMoreSettings, MenuItem::PlugAprs, MenuItem::PlugAprsText},
        11},
    {"Tools", "The log, the key test", "and the version", {MenuItem::ToolLog, MenuItem::ToolKeys, MenuItem::Version},
        3},
};
static_assert(std::size(Groups) == static_cast<std::size_t>(MenuGroup::Count));
// Two lines about an item for the menu's panel.
struct ItemAbout
{
    const char *line1;
    const char *line2;
};
ItemAbout itemAbout(MenuItem item)
{
    switch (item) {
    case MenuItem::Squelch:
        return {"Open, or 1 to 5: higher", "needs a stronger signal"};
    case MenuItem::SquelchMode:
        return {"Carrier opens on any", "signal, Tone on its tone"};
    case MenuItem::Power:
        return {"Transmit power of this", "channel, this session"};
    case MenuItem::FrequencyOffset:
        return {"Corrects the radio's", "frequency error"};
    case MenuItem::Roaming:
        return {"Use the roaming zone's", "repeaters on this channel"};
    case MenuItem::RoamZone:
        return {"The repeaters to move", "between, this session"};
    case MenuItem::RoamStatus:
        return {"Repeaters heard and", "their levels; search now"};
    case MenuItem::Backlight:
        return {"Display brightness", ""};
    case MenuItem::BacklightTime:
        return {"How long the light stays", "on after a key"};
    case MenuItem::BigName:
        return {"Large on the main screen:", "channel or contact name"};
    case MenuItem::BesidePower:
        return {"Next to the power: the", "battery voltage or volume"};
    case MenuItem::BatterySave:
        return {"Sleep between checks for", "a signal while quiet"};
    case MenuItem::KeyLock:
        return {"What the key lock covers", ""};
    case MenuItem::AutoLock:
        return {"Lock the keys after this", "long without use"};
    case MenuItem::Gps:
        return {"The GPS receiver", "on or off"};
    case MenuItem::GpsSystems:
        return {"Satellite systems the", "receiver uses"};
    case MenuItem::GpsStatus:
        return {"Position, satellites", "and time from the GPS"};
    case MenuItem::ClockZone:
        return {"Local time's offset", "from UTC"};
    case MenuItem::ClockShown:
        return {"The main screen's time:", "local or UTC"};
    case MenuItem::ClockCalibration:
        return {"Measure the clock's rate", "against GPS time"};
    case MenuItem::AprsSend:
        return {"Send the position once,", "now"};
    case MenuItem::AprsBeacon:
        return {"How often the position", "is sent by itself"};
    case MenuItem::AprsPower:
        return {"Transmit power of the", "APRS beacons"};
    case MenuItem::AprsReceive:
        return {"Listen for APRS stations", "(heard list under P2)"};
    case MenuItem::PlugChannels:
        return {"Every channel: names,", "frequencies, DMR, tones"};
    case MenuItem::PlugZones:
        return {"Zone names", ""};
    case MenuItem::PlugContacts:
        return {"Contact names", "and numbers"};
    case MenuItem::PlugGroups:
        return {"Receive group lists", ""};
    case MenuItem::PlugRepeaters:
        return {"The roaming zones'", "repeaters"};
    case MenuItem::PlugRoamZones:
        return {"Roaming zone names", ""};
    case MenuItem::PlugRadioIds:
        return {"The radio's own DMR IDs", ""};
    case MenuItem::PlugSettings:
        return {"The radio's settings;", "saving restarts it"};
    case MenuItem::PlugMoreSettings:
        return {"Further settings: GPS;", "saving restarts it"};
    case MenuItem::PlugAprs:
        return {"APRS station and beacon;", "saving restarts it"};
    case MenuItem::PlugAprsText:
        return {"The text sent with the", "position; restarts"};
    case MenuItem::ToolLog:
        return {"The radio's log; hold", "Exit for the next screen"};
    case MenuItem::ToolKeys:
        return {"Shows each key pressed;", "hold Exit for the next"};
    case MenuItem::Version:
    case MenuItem::Count:
        break;
    }
    return {"", ""};
}
MenuGroup menuGroup = MenuGroup::Channel;
// Showing the group list (true) or one group's items, and the row chosen.
bool menuAtGroups = true;
std::uint8_t menuRow = 0;
// The value in use when a value screen opened.
std::size_t menuValueInUse = 0;

// The codeplug editor's screens: the book listed, the record chosen in it
// and its place among those in use, the field chosen in the open record,
// and the value or text being chosen for it.
using plug::Book;
Book plugBook = Book::Channels;
std::ptrdiff_t plugRecord = -1;
std::size_t plugPlace = 0;
std::size_t plugCount = 0;
std::size_t plugField = 0;
std::size_t plugSaveRow = 0;
// Choosing a record for a link field, rather than opening one.
bool plugPicking = false;
std::int32_t plugValue = 0;
char plugTyped[plug::TextBytes] = {};
std::size_t plugTypedLength = 0;
std::size_t plugCursor = 0;
// Letters typed by pressing a key again and again.
Key plugLastKey = Key::Count;
os::Ticks plugLastTap = 0;
std::size_t plugTap = 0;
bool plugUpper = true;
// Texts for the rows and panels, off the ui task's stack.
char plugTexts[16][plug::TextBytes] = {};
// A saved record that the radio reads only at start: restart.
std::atomic<bool> restartWanted{false};
constexpr const char *BookNames[] = {"Channels", "Zones", "Contacts", "RX groups", "Repeaters", "Roam zones",
    "Radio IDs", "Settings", "More settings", "APRS", "APRS text"};
static_assert(std::size(BookNames) == static_cast<std::size_t>(Book::Count));
// The letters on each digit key, and the characters the knob steps through.
constexpr const char *KeyLetters[] = {" 0", ".,-_/1", "abc2", "def3", "ghi4", "jkl5", "mno6", "pqrs7", "tuv8",
    "wxyz9"};
constexpr std::string_view KnobChars =
    " ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789.,-_/()*#+:@!?";
constexpr os::Ticks TapMs = 1200;
// The items were opened straight from the standby screen: leaving them
// returns there.
bool menuQuick = false;
int backlightLevel = DefaultBacklight;
// Backlight time choices, in seconds.
constexpr std::uint16_t BacklightTimes[] = {0, 5, 10, 15, 20, 25, 30, 60, 120, 180, 240, 300, 900, 1800, 2700, 3600};
std::uint16_t backlightS = 0; // 0 = always on
bool backlightLit = true;
os::Ticks lastLightUse = 0;
std::uint8_t batterySave = 0;
// The GPS receiver as last reported, and the codeplug's choice at start.
GpsState gpsState;
bool gpsAtStart = false;
std::uint8_t gpsSystems = services::gps_systems::Gps | services::gps_systems::Beidou;
// Ids of the Calibrate list's information rows; the choices are minutes.
constexpr std::uint8_t CalibrationInfo = 250;
// The clock calibration as the gps task last reported it.
ClockCalibration clockCalibration{ClockCalibration::State::Stopped, 0, 0};
// Local time minus UTC, from the codeplug, in half-hour steps.
int timeZoneMinutes = 0;
constexpr int ZoneSteps = 53; // UTC-12:00 to UTC+14:00
// APRS beacons: the interval (the codeplug's at start) and the power.
constexpr std::uint16_t BeaconIntervals[] = {0, 60, 120, 300, 600, 900, 1200, 1800, 3600};
std::uint16_t beaconIntervalS = 0;
// The codeplug has an APRS station, so beacons can go out.
bool aprsStation = false;
// Receiving through the board, on from the start.
bool aprsReceive = true;
PowerLevel beaconPower = PowerLevel::Low;

// "UTC+3:00", "UTC-9:30", "UTC".
lib::StaticString<10> zoneText(int minutes)
{
    lib::StaticString<10> s;
    s << "UTC";
    if (minutes != 0) {
        const int m = minutes < 0 ? -minutes : minutes;
        s << (minutes < 0 ? '-' : '+') << m / 60 << ':' << lib::zeroPadded(static_cast<std::uint32_t>(m % 60), 2);
    }
    return s;
}

// The local time as "HH:MM", or empty while the clock does not know it.
// The time is shown in UTC ("11:32Z") instead of local time.
bool clockUtc = false;
// A DMR channel's large name is its contact's, not the channel's own.
bool bigContactName = false;
// The volume next to the power instead of the battery voltage.
bool volumeBesidePower = false;

// The time @p agoS seconds ago as "14:32", or "11:32Z" in UTC; empty while
// the clock does not know it.
lib::StaticString<7> clockText(std::uint32_t agoS = 0)
{
    lib::StaticString<7> s;
    if (const auto utc = system::utcNow()) {
        const std::int64_t at = static_cast<std::int64_t>(*utc) - agoS + (clockUtc ? 0 : timeZoneMinutes * 60);
        const services::DateTime t = services::dateTime(static_cast<std::uint32_t>(at));
        s << lib::zeroPadded(t.hour, 2) << ':' << lib::zeroPadded(t.minute, 2);
        if (clockUtc) {
            s << 'Z';
        }
    }
    return s;
}

// What happened, for the pages under P2: calls heard and made; and roaming,
// beacons and fixes. Each keeps what fits on its page.
services::EventLog<LogRows> callLog;
services::EventLog<LogRows> systemLog;
// The page P2 opens: 0 calls, 1 system, 2 APRS stations heard.
constexpr int LogPages = 3;
int logPage = 0;

std::uint32_t uptimeS()
{
    return static_cast<std::uint32_t>(os::nowMs() / 1000);
}
// The volume step 0..15, -1 until the knob has been read.
int volumeLevel = -1;
bool receiving = false;
// The DMR call on the call screen: while it is heard, then for a moment
// after it ends, unless a key or a channel change sends the screen back.
struct HeardCall
{
    bool shown = false;
    bool active = false;
    bool group = false;
    std::uint32_t source = 0;
    std::uint32_t destination = 0;
    // The caller's callsign (or contact name) and the user's name.
    char caller[CodeplugSource::ContactNameBytes + 1] = {};
    char callerName[CodeplugSource::ContactNameBytes + 1] = {};
    char target[CodeplugSource::ContactNameBytes + 1] = {};
    os::Ticks started = 0;
    os::Ticks ended = 0;
};
HeardCall heard;
char callChannel[Channel::NameBytes + 1] = {};
// Roaming: the roaming zone, how many repeaters it has, and the repeater
// the radio task last reported.
std::uint8_t roamingZone = services::RoamingSettings::NoZone;
std::size_t roamingSites = 0;
// Roaming switched on in the menu, for this session.
bool roamingOn = true;
// The frequency offset set in the menu, in kHz, for this session.
constexpr int MaxOffsetKhz = 5;
int offsetKhz = radio::DefaultFrequencyOffsetHz / 1000;
struct RoamState
{
    bool searching = false;
    int site = -1;
    std::uint32_t receiveHz = 0;
    std::uint8_t colourCode = 0;
    char name[CodeplugSource::SiteNameBytes + 1] = {};
};
RoamState roam;
// The working channel's contact as the screens name it ("TG 244", the
// contact's name), looked up again only when the contact changes.
char talkgroupText[16] = {};
char talkgroupName[CodeplugSource::ContactNameBytes + 1] = {};
services::dmr::Target talkgroupFor{services::dmr::CallType::All, 0xFFFFFFFF};
// A DMR channel with a contact to call: its screens show the talkgroup;
// others show the channel.
bool hasTalkgroup(const Channel &c)
{
    return c.digital() && (c.dmr.contact.id != 0 || c.dmr.contact.type == services::dmr::CallType::All);
}
// A call or FM transmission of the user's is on the air, since when.
bool transmitCall = false;
os::Ticks transmitStarted = 0;
// The radio task's roaming state, copied for the status screen and its
// menu row; kept here, not on the stack.
radio::RoamingView roamView;
// Hands the roaming zone's repeaters to the radio task, which then looks
// for one afresh.
void loadRoamingSites()
{
    radio::setRoamingSites([](std::span<services::RoamingSite> out) {
        roamingSites = roamingZone != services::RoamingSettings::NoZone && codeplug != nullptr
            ? codeplug->roamingSites(roamingZone, out)
            : 0;
        return roamingSites;
    });
}
// The repeater in use and how it is doing, as "2 Good", or what roaming
// is doing instead.
lib::StaticString<9> roamStateText(const radio::RoamingView &v)
{
    constexpr const char *States[] = {"?", "Good", "Weak", "Fail", "Lost"};
    lib::StaticString<9> t;
    if (!v.roaming) {
        t << "Off";
    } else if (v.searching) {
        t << "Search";
    } else if (v.site < 0) {
        t << "None";
    } else {
        t << v.site + 1 << ' ' << States[static_cast<int>(v.health)];
    }
    return t;
}
// Fills @p list with what roaming knows: the repeater in use, then each
// repeater heard with its level, the age of that level and its beacon
// period, and a row that searches now. The cursor stays on the same row.
constexpr std::uint8_t SearchNowRow = 0xFF;
void fillRoamStatus(ListPicker &list)
{
    radio::RoamingView &v = roamView;
    radio::roamingView(v);
    const std::uint8_t at = list.empty() ? 0 : list.chosen();
    list.open("Roaming");
    list.add(0, "In use", roamStateText(v).view());
    const std::uint32_t now = os::nowMs();
    for (std::uint8_t k = 0; k < v.count; ++k) {
        const services::roaming::HeardList::Entry &e = v.entries[k];
        // "2*438.2125 60.0": position, a star for the one in use, output
        // frequency, beacon period in seconds.
        lib::StaticString<24> text;
        const std::uint32_t frac = v.receiveHz[k] / 100 % 10'000;
        text << e.position + 1 << (e.position == v.site ? '*' : ' ') << v.receiveHz[k] / 1'000'000 << '.'
             << static_cast<char>('0' + frac / 1000) << static_cast<char>('0' + frac / 100 % 10)
             << static_cast<char>('0' + frac / 10 % 10);
        if (frac % 10 != 0) {
            text << static_cast<char>('0' + frac % 10);
        }
        if (e.periodCs != 0) {
            text << ' ' << e.periodCs / 100 << '.' << static_cast<char>('0' + e.periodCs % 100 / 10);
        }
        // "-35 12s": level and its age.
        const std::uint32_t age = (now - e.measuredMs) / 1000;
        lib::StaticString<12> value;
        value << e.levelDbm << ' ';
        if (age < 100) {
            value << age << 's';
        } else if (age < 6000) {
            value << age / 60 << 'm';
        } else {
            value << "old";
        }
        list.add(static_cast<std::uint8_t>(k + 1), text.view(), value.view());
    }
    list.add(SearchNowRow, "Search now");
    list.select(at);
}
// The side keys' functions, and whether a long press has already acted.
services::KeyFunction sideShort[CodeplugSettings::SideKeys] = {};
services::KeyFunction sideLong[CodeplugSettings::SideKeys] = {};
bool sideLongDone[CodeplugSettings::SideKeys] = {};
constexpr os::Ticks CallHoldMs = 5000;
constexpr std::uint32_t AllCallId = 0xFFFFFF;

template <std::size_t N>
void copyName(char (&out)[N], const char *text)
{
    std::size_t n = 0;
    for (; n + 1 < N && text[n] != '\0'; ++n) {
        out[n] = text[n];
    }
    out[n] = '\0';
}

void nameTalkgroup(const services::dmr::Target &t)
{
    if (t == talkgroupFor) {
        return;
    }
    talkgroupFor = t;
    lib::StaticString<16> text;
    if (t.type == services::dmr::CallType::All) {
        text << "ALL CALL";
    } else {
        text << (t.type == services::dmr::CallType::Group ? "TG " : "PC ") << t.id;
    }
    const std::size_t n = std::min(text.view().size(), sizeof(talkgroupText) - 1);
    std::copy_n(text.view().data(), n, talkgroupText);
    talkgroupText[n] = '\0';
    talkgroupName[0] = '\0';
    std::array<char, CodeplugSource::ContactNameBytes + 1> name{};
    if (t.type != services::dmr::CallType::All && codeplug != nullptr &&
        codeplug->contactName(t.id, t.type == services::dmr::CallType::Group, name)) {
        copyName(talkgroupName, name.data());
    }
}

// Names for a call from the codeplug: the caller's own contact entry
// first, else the user directory's callsign and name.
void nameCall(HeardCall &h, CodeplugSource *plug)
{
    h.caller[0] = h.callerName[0] = h.target[0] = '\0';
    if (plug == nullptr) {
        return;
    }
    std::array<char, CodeplugSource::ContactNameBytes + 1> name{};
    if (plug->contactName(h.destination, h.group, name)) {
        copyName(h.target, name.data());
    }
    // The user directory's callsign first, then a contact's name.
    CodeplugSource::DmrUser user;
    const bool known = plug->dmrUser(h.source, user);
    if (known && user.callsign[0] != '\0') {
        copyName(h.caller, user.callsign.data());
    } else if (plug->contactName(h.source, false, name)) {
        copyName(h.caller, name.data());
    } else if (known) {
        copyName(h.caller, user.name.data());
        return;
    }
    if (known && h.caller[0] != '\0' && std::string_view(user.name.data()) != h.caller) {
        copyName(h.callerName, user.name.data());
    }
}
// The standby screen's quick-key hint.
lib::StaticString<40> quickKeys;
// Frequency digits typed on the standby screen.
constexpr std::size_t EntryDigits = 6;
char entry[EntryDigits + 1] = {};
std::size_t entryLength = 0;
constexpr std::uint32_t VfoStepHz = 12'500;

// The key test: every key as a box in panel order, lit while pressed, and
// the knob values below.
class KeyTestView
{
public:
    void draw(Display &d)
    {
        d.fillRect(0, 0, d.width(), d.height(), colors::Black);
        drawText(d, HeaderFont, 0, 0, "Keys (Exit=log)", colors::Grey, colors::Black);
        drawLockMark(d);
        drawBattery(d);
        for (unsigned i = 0; i < static_cast<unsigned>(Key::Count); ++i) {
            drawKey(d, static_cast<Key>(i));
        }
        drawValues(d);
    }

    void setKeys(Display &d, KeyMask keys, bool visible)
    {
        const KeyMask changed = keys ^ m_keys;
        m_keys = keys;
        if (!visible) {
            return;
        }
        for (unsigned i = 0; i < static_cast<unsigned>(Key::Count); ++i) {
            if ((changed & (1u << i)) != 0) {
                drawKey(d, static_cast<Key>(i));
            }
        }
    }

    void setClicks(std::int32_t v) { m_clicks = v; }
    void setRaw(std::int32_t v) { m_raw = v; }
    void setVolume(std::int32_t v) { m_volume = v; }

    void drawValues(Display &d)
    {
        lib::StaticString<40> line;
        line << "Knob " << m_clicks << " (raw " << m_raw << ")";
        valueLine(d, ValuesTop, line.view());
        line.clear();
        if (m_volume < 0) {
            line << "Volume: none";
        } else {
            line << "Volume " << m_volume / 10 << "." << m_volume % 10 << " %";
        }
        valueLine(d, ValuesTop + LogFont.height, line.view());
    }

private:
    static constexpr int Columns = 6;
    static constexpr int BoxW = 26;
    static constexpr int BoxH = 13;
    static constexpr int GridTop = 11;
    static constexpr int ValuesTop = GridTop + 4 * (BoxH + 2) + 4;

    // Panel order: side keys and PTT, the four control keys, then digits.
    static constexpr Key Layout[] = {Key::Ptt, Key::Side1, Key::Side2, Key::Side3, Key::Front1, Key::Front2, Key::Menu, Key::Up,
        Key::Down, Key::Exit, Key::Star, Key::Hash, Key::Digit1, Key::Digit2, Key::Digit3, Key::Digit4, Key::Digit5,
        Key::Digit6, Key::Digit7, Key::Digit8, Key::Digit9, Key::Digit0};

    void drawKey(Display &d, Key key)
    {
        int slot = 0;
        while (Layout[slot] != key) {
            ++slot;
        }
        const int x = (slot % Columns) * (BoxW + 1);
        const int y = GridTop + (slot / Columns) * (BoxH + 2);
        const bool down = (m_keys & keyBit(key)) != 0;
        const Color fill = down ? colors::Green : rgb(48, 48, 56);
        const Color ink = down ? colors::Black : colors::White;
        d.fillRect(x, y, BoxW, BoxH, fill);
        const std::string_view name = keyText(key);
        const int textW = static_cast<int>(name.size()) * HeaderFont.width;
        drawText(d, HeaderFont, x + (BoxW - textW) / 2, y + 3, name, ink, fill);
    }

    static void valueLine(Display &d, int y, std::string_view text)
    {
        const int x = drawText(d, LogFont, 0, y, text, colors::White, colors::Black);
        d.fillRect(x, y, d.width() - x, LogFont.height, colors::Black);
    }

    KeyMask m_keys = 0;
    std::int32_t m_clicks = 0;
    std::int32_t m_raw = 0;
    std::int32_t m_volume = -1;
};

KeyTestView keyTest;

void showLog(Display &d)
{
    d.fillRect(0, 0, d.width(), d.height(), colors::Black);
    drawHeader(d);
    logView.draw(d, HeaderFont.height, true);
}

lib::StaticString<12> durationText(std::uint16_t seconds)
{
    lib::StaticString<12> text;
    if (seconds == 0) {
        text << "Always";
    } else if (seconds >= 3600) {
        text << seconds / 3600 << " h";
    } else if (seconds >= 60) {
        text << seconds / 60 << " min";
    } else {
        text << seconds << " s";
    }
    return text;
}

const char *batterySaveText(std::uint8_t mode)
{
    constexpr const char *Texts[] = {"Off", "On", "Fast wake"};
    return Texts[mode <= 2 ? mode : 1];
}

// A key, the knob, a signal or a transmission: the backlight comes on and
// stays on for its time.
void useLight(Display &d)
{
    lastLightUse = os::nowMs();
    if (!backlightLit) {
        backlightLit = true;
        d.setBacklight(backlightLevel);
        services::power::hold(services::power::AwakeReason::Display);
    }
}

void uiMain(void *)
{
    Display &d = *display;
    d.start();
    logView.configure(d.width() / LogFont.width, (d.height() - HeaderFont.height) / LogFont.height);
    drawStartScreen(d);
    d.setBacklight(DefaultBacklight);
    services::power::hold(services::power::AwakeReason::Display);
    lastLightUse = os::nowMs();
    log::info("ui") << "display " << d.width() << " x " << d.height();

    quickKeys << keyText(Key::Menu) << "  " << keyText(Key::Front1) << " Zone  " << keyText(Key::Front2) << " Log  "
              << keyText(Key::Hash) << " VFO";

    // The codeplug: settings, and the channel the radio starts on.
    if (codeplug != nullptr) {
        static ChannelSelector selector(*codeplug);
        selector.load();
        if (selector.current()) {
            channels = &selector;
            keyLock.setSettings(selector.settings().keyLock);
            backlightS = selector.settings().backlightS;
            batterySave = selector.settings().batterySave;
            gpsAtStart = selector.settings().gps;
            gpsSystems = selector.settings().gpsSystems;
            timeZoneMinutes = selector.settings().timeZoneMinutes;
            if (const auto a = codeplug->aprsSettings()) {
                beaconIntervalS = a->beaconIntervalS;
                beaconPower = a->power;
                aprsStation = a->callsign[0] != '\0';
                aprs::setStation(*a);
            }
            if (selector.settings().voiceEnhancement) {
                radio::post(radio::VoiceEnhancement{true});
            }
            const services::RoamingSettings &roaming = selector.settings().roaming;
            roamingZone = roaming.zone;
            loadRoamingSites();
            radio::post(radio::RoamingSetup{roaming});
            std::copy(std::begin(selector.settings().sideShort), std::end(selector.settings().sideShort), sideShort);
            std::copy(std::begin(selector.settings().sideLong), std::end(selector.settings().sideLong), sideLong);
            log::info("ui") << "codeplug: zone " << selector.zone() << " \"" << selector.zoneName() << "\", "
                            << selector.zoneSize() << " channels" << (selector.vfoMode() ? ", VFO" : "");
            logLockSettings();
        } else {
            log::warning("ui") << "codeplug: no channel to start on";
        }
    }

    View view = View::Start;
    const os::Ticks startUntil = os::nowMs() + StartScreenMs;
    bool startingShown = false;
    os::Ticks lastSecond = 0;
    os::Ticks noticeUntil = 0;
    bool noticeShown = false;
    bool volumeNoticeUp = false; // the notice up is the volume one
    constexpr os::Ticks NoticeMs = 1500;

    // The standby, log and key test screens take the lock; the zone list,
    // the test pattern and full-screen messages do not.
    auto lockContext = [&] {
        services::KeyLock::Context c;
        c.screenLockable = view == View::Standby || view == View::Gps || view == View::Log || view == View::KeyTest;
        c.transmitting = transmitting;
        c.nowMs = os::nowMs();
        return c;
    };
    auto notice = [&](std::string_view text) {
        volumeNoticeUp = false;
        drawNotice(d, text);
        noticeShown = true;
        noticeUntil = os::nowMs() + NoticeMs;
    };
    lib::StaticString<7> clockNow;
    lib::StaticString<24> roamSiteText;
    int roamLogged = -1;
    std::uint32_t fixLoggedS = 0;
    os::Ticks lastClockMinute = 0;
    os::Ticks lastHeardRefresh = 0;
    constexpr os::Ticks HeardRefreshMs = 10'000;
    bool gpsScreenNeed = false;
    // Battery save also decides whether the GPS receiver sleeps between fixes.
    auto setBatterySave = [&](std::uint8_t mode) {
        batterySave = mode;
        radio::post(radio::BatterySave{mode});
        gps::post(gps::PowerSave{mode != 0});
    };
    // The radio's chips are up and the start screen is gone.
    auto hardwareUp = [&] { return view != View::Start && radio::ready(); };
    // A key or knob turn refused because the chips are not up yet: on the
    // start screen silently, else with a notice once per press.
    auto refuseNotReady = [&](bool press) {
        if (view == View::Start || !press) {
            return;
        }
        log::info("ui") << "key refused: the radio is not ready";
        notice("Please wait...");
    };
    auto standbyState = [&] {
        StandbyState s;
        s.transmitting = transmitting;
        s.receiving = receiving;
        s.locked = keyLock.locked() || keyLock.settings().forced;
        s.entry = std::string_view(entry, entryLength);
        s.volume = volumeLevel;
        s.volumeShown = volumeBesidePower;
        s.millivolts = batteryState.millivolts;
        s.batteryLow = batteryState.low;
        s.keys = quickKeys.view();
        s.gps = gpsState;
        clockNow = clockText();
        s.clock = clockNow.view();
        s.beacons = aprsStation && beaconIntervalS != 0;
        const std::optional<Channel> ch = channels != nullptr ? channels->current() : std::nullopt;
        s.roaming = roamingOn && ch && ch->digital() && ch->roaming && roamingSites > 0;
        s.roamSearching = roam.searching;
        s.roamHz = roam.receiveHz;
        s.roamColourCode = roam.colourCode;
        roamSiteText.clear();
        if (roam.name[0] != '\0') {
            roamSiteText << roam.name;
        } else if (roam.site >= 0) {
            roamSiteText << "repeater " << roam.site + 1;
        }
        s.roamSite = roamSiteText.view();
        if (ch && hasTalkgroup(*ch)) {
            nameTalkgroup(ch->dmr.contact);
            s.talkgroup = talkgroupText;
            s.talkgroupName = talkgroupName;
            s.contactNameLarge = bigContactName;
        }
        return s;
    };
    // The transmit screen: the talkgroup on DMR, the channel on FM.
    lib::StaticString<12> transmitTarget;
    auto transmitInfo = [&] {
        constexpr const char *Powers[] = {"LOW", "MID", "HIGH", "TURBO"};
        TransmitInfo t;
        t.seconds = static_cast<std::uint32_t>((os::nowMs() - transmitStarted) / 1000);
        const std::optional<Channel> ch = channels != nullptr ? channels->current() : std::nullopt;
        if (!ch) {
            return t;
        }
        t.power = Powers[static_cast<int>(ch->power) & 3];
        if (ch->digital()) {
            t.slot = ch->dmr.slot;
        }
        copyName(callChannel, ch->name);
        if (hasTalkgroup(*ch)) {
            nameTalkgroup(ch->dmr.contact);
            t.target = bigContactName && talkgroupName[0] != '\0' ? std::string_view(talkgroupName)
                : callChannel[0] != '\0'                          ? std::string_view(callChannel)
                                                                  : std::string_view(talkgroupText);
            t.name = talkgroupText;
        } else {
            copyName(callChannel, ch->name);
            t.target = callChannel[0] != '\0' ? std::string_view(callChannel) : std::string_view("FM");
        }
        return t;
    };
    auto callInfo = [&] {
        CallInfo c;
        c.active = heard.active;
        c.allCall = heard.destination == AllCallId;
        c.group = heard.group;
        c.callerName = heard.callerName;
        c.callerId = heard.source;
        c.targetName = heard.target;
        c.targetId = heard.destination;
        c.callerHeadline = heard.caller;
        const std::optional<Channel> ch = channels != nullptr ? channels->current() : std::nullopt;
        if (ch && ch->receiveHz != ch->transmitHz) {
            c.slot = ch->dmr.slot;
        }
        callChannel[0] = '\0';
        if (ch) {
            copyName(callChannel, ch->name);
        }
        c.channel = callChannel;
        c.seconds = static_cast<std::uint32_t>(((heard.active ? os::nowMs() : heard.ended) - heard.started) / 1000);
        return c;
    };
    auto barHeader = [&](bool bar) {
        headerGround = colors::Black;
        // Above the status row's rule, so the battery never covers it.
        headerHeight = bar ? StandbyScreen::StatusH - 1 : HeaderFont.height;
        headerTop = bar ? 2 : 0;
    };
    // The calls or system page: what happened, newest first, a row per
    // event. Names are looked up as the rows are drawn; the logs hold
    // numbers.
    auto drawActivity = [&] {
        const auto &log = logPage == 0 ? callLog : systemLog;
        drawLogFrame(d, logPage == 0 ? "Calls" : "System", logPage == 0 ? "1/3" : "2/3", "Up/Down: page   P2: back");
        const std::uint32_t now = uptimeS();
        for (int row = 0; row < LogRows; ++row) {
            const auto *e = log.newest(static_cast<std::size_t>(row));
            if (e == nullptr) {
                const bool empty = row == 0 && log.count() == 0;
                drawLogRow(d, row, {}, empty ? "Nothing yet" : "", palette::Dim);
                continue;
            }
            lib::StaticString<7> when = clockText(now - e->seconds());
            if (when.view().empty()) {
                when << '-' << (now - e->seconds()) / 60 << 'm';
            }
            lib::StaticString<40> what;
            Color ink = palette::Text;
            using Kind = services::EventKind;
            switch (e->kind()) {
            case Kind::CallIn: {
                // Off the UI task's small stack.
                static HeardCall who;
                who.source = e->a();
                who.destination = e->b();
                who.group = e->group();
                nameCall(who, codeplug);
                what << "RX ";
                if (who.caller[0] != '\0') {
                    what << who.caller;
                } else {
                    what << e->a();
                }
                what << "  " << (e->group() ? "TG" : "PC") << e->b() << "  " << e->value() << 's';
                ink = palette::Receive;
                break;
            }
            case Kind::CallOut:
                what << "TX ";
                if (e->a() == 1) {
                    what << "FM " << e->b() / 1000 << '.' << lib::zeroPadded(e->b() % 1000, 3);
                } else {
                    what << (e->group() ? "TG" : "PC") << e->b();
                }
                what << "  " << e->value() << 's';
                ink = palette::Transmit;
                break;
            case Kind::RoamMove:
                what << "ROAM repeater " << e->a();
                break;
            case Kind::RoamLost:
                what << "ROAM repeater " << e->a() << " lost";
                ink = palette::Warn;
                break;
            case Kind::AprsBeacon:
                what << "APRS beacon sent";
                ink = palette::Beacon;
                break;
            case Kind::GpsFix:
                what << "GPS fix";
                ink = palette::Dim;
                break;
            case Kind::GpsLost:
                what << "GPS fix lost";
                ink = palette::Dim;
                break;
            }
            drawLogRow(d, row, when.view(), what.view(), ink);
        }
    };
    // The APRS page: the stations heard, newest first, with the time last
    // heard and the distance; the chosen one stands out.
    auto drawHeard = [&] {
        drawLogFrame(d, "APRS heard", "3/3", "Up/Down: page   Menu: open");
        if (stationShown < heardTop) {
            heardTop = stationShown;
        } else if (stationShown >= heardTop + LogRows) {
            heardTop = stationShown + 1 - LogRows;
        }
        const std::uint32_t now = os::nowMs();
        for (int row = 0; row < LogRows; ++row) {
            const std::size_t i = heardTop + static_cast<std::size_t>(row);
            aprs::HeardStation s;
            if (i >= heardShownCount || !aprs::heardStation(heardCalls[i], s)) {
                const bool empty = row == 0 && heardShownCount == 0;
                drawLogRow(d, row, {}, empty ? (aprsReceive ? "Nothing yet" : "Receive is off") : "", palette::Dim);
                continue;
            }
            const std::uint32_t agoS = (now - s.heardAtMs) / 1000;
            lib::StaticString<7> when = clockText(agoS);
            if (when.view().empty()) {
                when << '-' << agoS / 60 << 'm';
            }
            lib::StaticString<40> what;
            what << s.call;
            for (std::size_t n = what.size(); n < aprs::HeardStation::CallBytes + 1; ++n) {
                what << ' ';
            }
            what << distanceText(gpsState, s).view();
            drawLogRow(d, row, when.view(), what.view(), palette::Text, i == stationShown);
        }
    };
    // Logs an event, and shows it at once when the list is open.
    auto logEvent = [&](services::EventKind kind, std::uint32_t a = 0, std::uint32_t b = 0, unsigned value = 0,
                        bool group = false) {
        const bool call = kind == services::EventKind::CallIn || kind == services::EventKind::CallOut;
        (call ? callLog : systemLog).add(kind, uptimeS(), a, b, value, group);
        if (view == View::Activity && logPage == (call ? 0 : 1) && !noticeShown) {
            drawActivity();
        }
    };
    auto itemRow = [&](MenuItem item, const char *&label, lib::StaticString<12> &value) {
        constexpr const char *Powers[] = {"Low", "Mid", "High", "Turbo"};
        const services::KeyLockSettings &k = keyLock.settings();
        switch (item) {
        case MenuItem::Squelch:
            label = "Squelch";
            if (channels->squelch() == 0) {
                value << "Open";
            } else {
                value << channels->squelch();
            }
            break;
        case MenuItem::SquelchMode:
            label = "Squelch type";
            value << (channels->current() && channels->current()->squelchMode == SquelchMode::Tone ? "Tone" : "Carrier");
            break;
        case MenuItem::Power:
            label = "Power";
            value << (channels->current() ? Powers[static_cast<int>(channels->current()->power)] : "-");
            break;
        case MenuItem::Roaming:
            label = "Roaming";
            value << (roamingOn ? "On" : "Off");
            break;
        case MenuItem::RoamStatus: {
            label = "Roam status";
            radio::roamingView(roamView);
            value << roamStateText(roamView).view();
            break;
        }
        case MenuItem::RoamZone: {
            label = "Roam zone";
            std::array<char, CodeplugSource::ZoneNameBytes + 1> name{};
            value << (roamingZone != services::RoamingSettings::NoZone && codeplug->roamingZoneName(roamingZone, name)
                    ? std::string_view(name.data())
                    : std::string_view("-"));
            break;
        }
        case MenuItem::FrequencyOffset:
            label = "Freq offset";
            value << (offsetKhz > 0 ? "+" : "") << offsetKhz << " kHz";
            break;
        case MenuItem::Backlight:
            label = "Backlight";
            value << backlightLevel;
            break;
        case MenuItem::BacklightTime:
            label = "Light time";
            value << durationText(backlightS).view();
            break;
        case MenuItem::BigName:
            label = "Big name";
            value << (bigContactName ? "Contact" : "Channel");
            break;
        case MenuItem::BesidePower:
            label = "Next to power";
            value << (volumeBesidePower ? "Volume" : "Voltage");
            break;
        case MenuItem::BatterySave:
            label = "Battery save";
            value << batterySaveText(batterySave);
            break;
        case MenuItem::KeyLock:
            label = "Lock covers";
            value << (k.knob ? "All" : k.keypad && k.side ? "Keys+PTT"
                    : k.keypad                            ? "Keypad"
                                                          : "Side+PTT");
            break;
        case MenuItem::AutoLock:
            label = "Auto lock";
            if (k.autoLockMs == 0) {
                value << "Off";
            } else {
                value << k.autoLockMs / 1000 << " s";
            }
            break;
        case MenuItem::Gps:
            label = "GPS";
            value << (gpsState.on ? "On" : "Off");
            break;
        case MenuItem::GpsSystems:
            label = "Systems";
            value << services::gps_systems::name(gpsSystems);
            break;
        case MenuItem::ClockZone:
            label = "Time zone";
            value << zoneText(timeZoneMinutes).view();
            break;
        case MenuItem::ClockShown:
            label = "Show time";
            value << (clockUtc ? "UTC" : "Local");
            break;
        case MenuItem::ClockCalibration:
            label = "Calibrate";
            if (clockCalibration.state == ClockCalibration::State::Running) {
                value << clockCalibration.minutesDone << '/' << clockCalibration.minutes << " min";
            } else if (system::clockRate() == 0) {
                value << "None";
            } else {
                value << system::ppmText(system::clockRate()).view();
            }
            break;
        case MenuItem::GpsStatus:
            label = "GPS status";
            value << (!gpsState.on ? "Off" : gpsState.asleep ? "Sleeping"
                    : gpsState.fix.valid                     ? "Fix"
                    : gpsState.receiving                     ? "Searching"
                                                             : "No data");
            break;
        case MenuItem::AprsBeacon:
            label = "Beacon";
            if (beaconIntervalS == 0) {
                value << "Off";
            } else {
                value << durationText(beaconIntervalS).view();
            }
            break;
        case MenuItem::AprsPower:
            label = "Beacon power";
            value << Powers[static_cast<int>(beaconPower) & 3];
            break;
        case MenuItem::AprsSend:
            label = "Send beacon";
            break;
        case MenuItem::AprsReceive:
            label = "APRS receive";
            value << (aprsReceive ? "On" : "Off");
            break;
        case MenuItem::PlugChannels:
        case MenuItem::PlugZones:
        case MenuItem::PlugContacts:
        case MenuItem::PlugGroups:
        case MenuItem::PlugRepeaters:
        case MenuItem::PlugRoamZones:
        case MenuItem::PlugRadioIds:
        case MenuItem::PlugSettings:
        case MenuItem::PlugMoreSettings:
        case MenuItem::PlugAprs:
        case MenuItem::PlugAprsText:
            label = BookNames[static_cast<int>(item) - static_cast<int>(MenuItem::PlugChannels)];
            break;
        case MenuItem::ToolLog:
            label = "Log";
            break;
        case MenuItem::ToolKeys:
            label = "Key test";
            break;
        case MenuItem::Version:
            label = "Version";
            break;
        case MenuItem::Count:
            break;
        }
    };
    // Whether Menu on @p item opens a screen of its own rather than values.
    auto itemOpens = [](MenuItem item) {
        return item == MenuItem::RoamStatus || item == MenuItem::GpsStatus || item == MenuItem::ToolLog ||
            item == MenuItem::ToolKeys || (item >= MenuItem::PlugChannels && item <= MenuItem::PlugAprsText);
    };
    // The menu as it stands: the groups, or one group's items with their
    // values, five rows around the chosen one and a panel about it; the
    // status strip too unless only the cursor moved.
    auto drawMenu = [&](bool strip = true) {
        const GroupInfo &g = Groups[static_cast<int>(menuGroup)];
        const std::size_t count = menuAtGroups ? std::size(Groups) : g.count;
        if (menuRow >= count) {
            menuRow = 0;
        }
        std::size_t first = 0;
        if (count > static_cast<std::size_t>(MenuRows) && menuRow > MenuRows / 2) {
            first = std::min<std::size_t>(menuRow - MenuRows / 2, count - MenuRows);
        }
        MenuRow rows[MenuRows];
        lib::StaticString<12> values[MenuRows];
        int shown = 0;
        for (; shown < MenuRows && first + static_cast<std::size_t>(shown) < count; ++shown) {
            const std::size_t i = first + static_cast<std::size_t>(shown);
            if (menuAtGroups) {
                rows[shown] = {Groups[i].name, {}, true};
                continue;
            }
            const char *label = "";
            itemRow(g.items[i], label, values[shown]);
            rows[shown] = {label, values[shown].view(), itemOpens(g.items[i])};
        }
        std::string_view about1;
        std::string_view about2;
        static lib::StaticString<24> uptime;
        if (menuAtGroups) {
            about1 = Groups[menuRow].about1;
            about2 = Groups[menuRow].about2;
        } else if (g.items[menuRow] == MenuItem::Version) {
            uptime.clear();
            uptime << "up " << os::nowMs() / 60'000 << " min";
            about1 = Version;
            about2 = uptime.view();
        } else {
            const ItemAbout a = itemAbout(g.items[menuRow]);
            about1 = a.line1;
            about2 = a.line2;
        }
        std::string_view hint = "Menu: change   Exit: back";
        if (menuAtGroups || itemOpens(g.items[menuRow])) {
            hint = "Menu: open   Exit: back";
        } else if (g.items[menuRow] == MenuItem::AprsSend) {
            hint = "Menu: send   Exit: back";
        } else if (g.items[menuRow] == MenuItem::Version) {
            hint = "Exit: back";
        }
        lib::StaticString<24> title;
        if (menuAtGroups) {
            title << "Menu";
        } else {
            title << "Menu > " << g.name;
        }
        if (strip) {
            standby.drawStatus(d, standbyState());
            drawBattery(d);
        }
        drawMenuScreen(d, title.view(), menuRow, count, rows, shown, static_cast<int>(menuRow - first), about1,
            about2, hint);
    };
    auto moveMenu = [&](int steps) {
        const int count = static_cast<int>(menuAtGroups ? std::size(Groups) : Groups[static_cast<int>(menuGroup)].count);
        menuRow = static_cast<std::uint8_t>(((menuRow + steps) % count + count) % count);
        drawMenu(false);
    };
    // A value screen: large buttons for a few values, a large spinner for
    // many, and a list where the values carry notes.
    auto menuValueSpinner = [] { return !menuValue.hasValues() && menuValue.size() > 4; };
    auto drawMenuValue = [&](bool strip = true) {
        if (strip) {
            standby.drawStatus(d, standbyState());
            drawBattery(d);
        }
        constexpr std::string_view Hint = "Menu: set   Exit: back";
        if (menuValueSpinner()) {
            drawValueSpinner(d, menuValue, menuValueInUse, Hint);
        } else if (menuValue.hasValues()) {
            menuValue.draw(d, StandbyScreen::StatusH, Hint);
        } else {
            drawValueButtons(d, menuValue, menuValueInUse, Hint);
        }
    };
    // The key hint under the zone list and the roaming status.
    auto listHint = [&]() -> std::string_view {
        return view == View::RoamStatus ? "Menu: search   Exit: back" : "Menu: use   Exit: back";
    };
    // The working screen is up, not a call or transmit screen over it.
    auto workingScreen = [&] { return view == View::Standby && !(transmitting && transmitCall) && !heard.shown; };
    // The codeplug editor's screens. Their texts live in plugTexts.
    auto plugText = [](std::size_t k) { return plug::Text(plugTexts[k], plug::TextBytes); };
    auto plugView = [](std::size_t k) { return std::string_view(plugTexts[k]); };
    auto plugStrip = [&] {
        standby.drawStatus(d, standbyState());
        drawBattery(d);
    };
    // A book's records around the chosen one, or the records a link field
    // may point to.
    auto drawPlugList = [&](bool strip = true) {
        if (strip) {
            plugStrip();
        }
        std::ptrdiff_t first = plugRecord;
        int chosen = 0;
        while (chosen < ListPicker::Rows / 2) {
            const std::ptrdiff_t before = first > 0 ? plugEditor->nearest(plugBook, first - 1, -1) : -1;
            if (before < 0) {
                break;
            }
            first = before;
            ++chosen;
        }
        MenuRow rows[ListPicker::Rows];
        int n = 0;
        for (std::ptrdiff_t i = first; i >= 0 && n < ListPicker::Rows; i = plugEditor->nearest(plugBook, i + 1, 1)) {
            plugEditor->title(plugBook, static_cast<std::size_t>(i), plugText(n));
            lib::TextWriter w(std::span(plugTexts[ListPicker::Rows + n], plug::TextBytes - 1));
            w << '#' << i + 1;
            plugTexts[ListPicker::Rows + n][w.size()] = '\0';
            rows[n] = {plugView(n), plugView(ListPicker::Rows + n)};
            ++n;
        }
        lib::StaticString<32> title;
        if (plugPicking) {
            plugEditor->label(plugField, plugText(15));
            title << plugView(15);
        } else {
            title << BookNames[static_cast<int>(plugBook)];
        }
        drawListScreen(d, title.view(), plugPlace, plugCount, rows, n, chosen,
            plugPicking ? "Menu: pick  #: none  Exit: back" : "Menu: open   Exit: back");
    };
    // The open record's fields, five around the chosen one, with a panel
    // saying what the chosen one takes, or what it was before a change.
    auto drawPlugEdit = [&](bool strip = true) {
        if (strip) {
            plugStrip();
        }
        const std::size_t count = plugEditor->fields();
        std::size_t first = 0;
        if (count > static_cast<std::size_t>(MenuRows) && plugField > MenuRows / 2) {
            first = std::min<std::size_t>(plugField - MenuRows / 2, count - MenuRows);
        }
        MenuRow rows[MenuRows];
        int n = 0;
        for (; n < MenuRows && first + static_cast<std::size_t>(n) < count; ++n) {
            const std::size_t i = first + static_cast<std::size_t>(n);
            plugEditor->label(i, plugText(n));
            plugEditor->show(i, plugText(MenuRows + n));
            rows[n] = {plugView(n), plugView(MenuRows + n), false, plugEditor->changed(i)};
        }
        const plug::FieldInfo f = plugEditor->field(plugField);
        lib::StaticString<40> about1;
        lib::StaticString<40> about2;
        about1 << f.page;
        if (f.readOnly) {
            about1 << ", read only";
        }
        if (plugEditor->changed(plugField)) {
            plugEditor->show(plugField, plugText(12), true);
            about2 << "Was " << plugView(12);
        } else {
            switch (f.kind) {
            case plug::FieldKind::Choice:
                about2 << plugEditor->choices(plugField) << " choices";
                break;
            case plug::FieldKind::Number:
                plugEditor->showValue(plugField, f.min, plugText(12));
                plugEditor->showValue(plugField, f.max, plugText(13));
                about2 << plugView(12) << " to " << plugView(13);
                break;
            case plug::FieldKind::Frequency:
                about2 << "MHz, from the keypad";
                break;
            case plug::FieldKind::Text:
                about2 << "Up to " << f.maxChars << " letters";
                break;
            case plug::FieldKind::Link:
                about2 << "One of the " << BookNames[static_cast<int>(f.link)];
                break;
            case plug::FieldKind::Byte:
                about2 << "A byte of the record";
                break;
            }
        }
        plugEditor->title(plugEditor->book(), plugEditor->index(), plugText(14));
        lib::StaticString<24> title;
        title << plugView(14);
        if (plugEditor->changes() > 0) {
            title << '*';
        }
        drawMenuScreen(d, title.view(), plugField, count, rows, n, static_cast<int>(plugField - first), about1.view(),
            about2.view(), "Menu: edit P2: page Exit: done");
    };
    // How many values the chosen field steps through on the value screen,
    // and where a value is among them.
    auto plugSteps = [&](const plug::FieldInfo &f) -> std::size_t {
        return f.kind == plug::FieldKind::Choice ? plugEditor->choices(plugField)
                                                 : static_cast<std::size_t>((f.max - f.min) / f.step + 1);
    };
    auto drawPlugValue = [&](bool strip = true) {
        if (strip) {
            plugStrip();
        }
        const plug::FieldInfo f = plugEditor->field(plugField);
        const std::size_t count = plugSteps(f);
        const auto index = static_cast<std::size_t>((plugValue - f.min) / f.step);
        plugEditor->label(plugField, plugText(15));
        constexpr std::string_view Hint = "Menu: set   Exit: back";
        if (f.kind == plug::FieldKind::Choice && count <= 4) {
            std::string_view labels[4];
            for (std::size_t k = 0; k < count; ++k) {
                plugEditor->showValue(plugField, static_cast<std::int32_t>(k), plugText(k));
                labels[k] = plugView(k);
            }
            drawValueButtons(d, plugView(15), labels, count, index,
                static_cast<std::size_t>(plugEditor->value(plugField)), Hint);
            return;
        }
        plugEditor->showValue(plugField, plugValue, plugText(0));
        plugEditor->show(plugField, plugText(1));
        drawValueSpinner(d, plugView(15), plugView(0), index, count, plugView(1),
            plugValue == plugEditor->value(plugField), Hint);
    };
    // A frequency shows as MHz with five decimals; a number as its digits.
    auto plugDigitsText = [&](lib::StaticString<16> &out) -> std::size_t {
        const bool frequency = plugEditor->field(plugField).kind == plug::FieldKind::Frequency;
        if (!frequency) {
            if (plugTypedLength > 0) {
                out << std::string_view(plugTyped, plugTypedLength);
            } else {
                out << plugValue;
            }
            return plugTypedLength > 0 ? plugTypedLength : 0;
        }
        lib::StaticString<12> digits;
        digits << lib::zeroPadded(static_cast<std::uint32_t>(std::max<std::int32_t>(plugValue, 0) / 10), 8);
        for (std::size_t k = 0; k < 8; ++k) {
            if (k == 3) {
                out << '.';
            }
            out << (k < plugTypedLength ? plugTyped[k] : digits.view()[k]);
        }
        return plugTypedLength + (plugTypedLength > 3 ? 1 : 0);
    };
    auto drawPlugDigits = [&](bool strip = true) {
        if (strip) {
            plugStrip();
        }
        const bool frequency = plugEditor->field(plugField).kind == plug::FieldKind::Frequency;
        lib::StaticString<16> text;
        const std::size_t typed = plugDigitsText(text);
        plugEditor->label(plugField, plugText(15));
        drawDigitsEntry(d, plugView(15), text.view(), typed, frequency ? "MHz" : "",
            frequency ? "Knob: 12.5 kHz steps" : "Knob: one up or down", "*: delete a digit",
            "0-9  *: delete  Menu: set");
    };
    auto drawPlugText = [&](bool strip = true) {
        if (strip) {
            plugStrip();
        }
        const plug::FieldInfo f = plugEditor->field(plugField);
        std::string_view letters;
        if (plugLastKey >= Key::Digit0 && plugLastKey <= Key::Digit9) {
            const char *set = KeyLetters[static_cast<int>(plugLastKey) - static_cast<int>(Key::Digit0)];
            std::size_t k = 0;
            for (; set[k] != '\0'; ++k) {
                const char c = set[k];
                plugTexts[15][k] = plugUpper && c >= 'a' && c <= 'z' ? static_cast<char>(c - 'a' + 'A') : c;
            }
            plugTexts[15][k] = '\0';
            letters = plugView(15);
        }
        plugEditor->label(plugField, plugText(14));
        drawTextEntry(d, plugView(14), std::string_view(plugTyped, plugTypedLength),
            std::min<std::size_t>(plugCursor, f.maxChars - 1), f.maxChars, letters, plugTap,
            plugUpper ? "#: ABC *: del Menu: set" : "#: abc *: del Menu: set");
    };
    // The changed fields with their new values; the panel shows the chosen
    // one's old value.
    auto plugChanged = [&](std::size_t k) -> std::ptrdiff_t {
        for (std::size_t i = 0; i < plugEditor->fields(); ++i) {
            if (plugEditor->listed(i) && k-- == 0) {
                return static_cast<std::ptrdiff_t>(i);
            }
        }
        return -1;
    };
    auto drawPlugSave = [&](bool strip = true) {
        if (strip) {
            plugStrip();
        }
        const std::size_t count = plugEditor->changes();
        plugSaveRow = std::min(plugSaveRow, count > 0 ? count - 1 : 0);
        std::size_t first = 0;
        if (count > static_cast<std::size_t>(MenuRows) && plugSaveRow > MenuRows / 2) {
            first = std::min<std::size_t>(plugSaveRow - MenuRows / 2, count - MenuRows);
        }
        MenuRow rows[MenuRows];
        int n = 0;
        for (; n < MenuRows && first + static_cast<std::size_t>(n) < count; ++n) {
            const std::ptrdiff_t i = plugChanged(first + static_cast<std::size_t>(n));
            plugEditor->label(static_cast<std::size_t>(i), plugText(n));
            plugEditor->show(static_cast<std::size_t>(i), plugText(MenuRows + n));
            rows[n] = {plugView(n), plugView(MenuRows + n), false, true};
        }
        lib::StaticString<40> was;
        if (const std::ptrdiff_t i = plugChanged(plugSaveRow); i >= 0) {
            plugEditor->show(static_cast<std::size_t>(i), plugText(12), true);
            was << "Was " << plugView(12);
        }
        lib::StaticString<24> changes;
        changes << count << (count == 1 ? " change" : " changes");
        plugEditor->title(plugEditor->book(), plugEditor->index(), plugText(14));
        lib::StaticString<24> title;
        title << "Save " << plugView(14) << '?';
        drawMenuScreen(d, title.view(), plugSaveRow, count, rows, n, static_cast<int>(plugSaveRow - first), was.view(),
            changes.view(), "Menu: save Exit: edit #: drop");
    };
    auto redrawView = [&] {
        // A full redraw covers any notice: the next one is drawn whole.
        noticeShown = false;
        volumeNoticeUp = false;
        const bool radioScreen = view == View::Standby || isList(view) || view == View::Gps || view == View::Heard ||
            view == View::Station || view == View::Activity;
        barHeader(radioScreen);
        if (view == View::Standby && transmitting && transmitCall) {
            standby.drawTransmit(d, standbyState(), transmitInfo());
            drawBattery(d);
        } else if (view == View::Activity) {
            standby.drawStatus(d, standbyState());
            drawBattery(d);
            drawActivity();
        } else if (view == View::Standby && heard.shown) {
            standby.drawCall(d, standbyState(), callInfo());
            drawBattery(d);
        } else if (view == View::Standby) {
            standby.draw(d, *channels, standbyState());
            drawBattery(d);
        } else if (view == View::Gps) {
            standby.drawStatus(d, standbyState());
            drawBattery(d);
            drawGpsScreen(d, gpsState, true);
        } else if (view == View::Station) {
            standby.drawStatus(d, standbyState());
            drawBattery(d);
            aprs::HeardStation s;
            if (stationShown < heardShownCount && aprs::heardStation(heardCalls[stationShown], s)) {
                drawAprsStation(d, s, gpsState, os::nowMs());
            }
        } else if (view == View::Heard) {
            standby.drawStatus(d, standbyState());
            drawBattery(d);
            drawHeard();
        } else if (view == View::Menu) {
            drawMenu();
        } else if (view == View::MenuValue) {
            drawMenuValue();
        } else if (view == View::PlugList) {
            drawPlugList();
        } else if (view == View::PlugEdit) {
            drawPlugEdit();
        } else if (view == View::PlugValue) {
            drawPlugValue();
        } else if (view == View::PlugDigits) {
            drawPlugDigits();
        } else if (view == View::PlugText) {
            drawPlugText();
        } else if (view == View::PlugSave) {
            drawPlugSave();
        } else if (isList(view)) {
            standby.drawStatus(d, standbyState());
            drawBattery(d);
            listOf(view).draw(d, StandbyScreen::StatusH, listHint());
        } else if (view == View::Log) {
            showLog(d);
        } else if (view == View::KeyTest) {
            keyTest.draw(d);
        }
    };
    auto show = [&](View v) {
        view = v;
        redrawView();
    };
    // The next screen in Exit-held order: standby, log, key test.
    auto nextView = [&] {
        if (view == View::Standby || view == View::Gps) {
            return View::Log;
        }
        if (view == View::Log) {
            return View::KeyTest;
        }
        return channels != nullptr ? View::Standby : View::Log;
    };
    using Verdict = services::KeyLock::Verdict;
    auto refreshLockMark = [&] {
        if (view == View::Log || view == View::KeyTest) {
            drawLockMark(d);
        } else if (view == View::Standby || isList(view) || view == View::Gps || view == View::Activity) {
            standby.drawStatus(d, standbyState());
            drawBattery(d);
        }
    };
    // One item's label and current value.
    // The top of the menu: the groups.
    auto openMenu = [&] {
        menuRow = static_cast<std::uint8_t>(menuGroup);
        menuAtGroups = true;
        menuQuick = false;
        view = View::Menu;
        redrawView();
    };
    // One group's settings, each with its current value.
    auto openGroup = [&](MenuGroup group, bool quick) {
        const GroupInfo &g = Groups[static_cast<int>(group)];
        menuGroup = group;
        menuRow = 0;
        for (std::uint8_t i = 0; i < g.count; ++i) {
            if (g.items[i] == menuItem) {
                menuRow = i;
            }
        }
        menuAtGroups = false;
        menuQuick = quick;
        view = View::Menu;
        redrawView();
    };
    // Redraws the GPS group's values when it is open, keeping the cursor.
    auto refreshGpsGroup = [&] {
        if (view == View::Menu && !menuAtGroups && menuGroup == MenuGroup::Position && !noticeShown) {
            drawMenu();
        }
    };
    // The heard list, newest first. The station named @p keep stays
    // chosen.
    auto openHeard = [&](std::string_view keep) {
        char kept[aprs::HeardStation::CallBytes + 1] = {};
        keep.copy(kept, sizeof kept - 1);
        heardShownCount = 0;
        stationShown = 0;
        aprs::HeardStation s;
        for (std::size_t i = 0; i < aprs::MaxHeard && aprs::heardStation(i, s); ++i) {
            std::memcpy(heardCalls[i], s.call, sizeof s.call);
            heardShownCount = i + 1;
            if (std::string_view(kept) == s.call) {
                stationShown = i;
            }
        }
        view = View::Heard;
        redrawView();
    };
    auto reopenHeard = [&] {
        openHeard(stationShown < heardShownCount ? std::string_view(heardCalls[stationShown]) : std::string_view());
    };
    // Page @p page under P2, counting round: calls, system, APRS heard.
    auto openLogPage = [&](int page) {
        logPage = (page % LogPages + LogPages) % LogPages;
        if (logPage == 2) {
            openHeard({});
        } else {
            show(View::Activity);
        }
    };
    auto useChannel = [&] {
        if (channels == nullptr || !channels->current()) {
            return;
        }
        const Channel &c = *channels->current();
        static std::uint32_t groups[CodeplugSource::MaxReceiveGroups];
        std::size_t n = 0;
        if (c.digital() && c.dmr.groupList != services::dmr::ChannelSettings::NoGroupList && codeplug != nullptr) {
            n = codeplug->receiveGroups(c.dmr.groupList, groups);
        }
        radio::setReceiveGroups(std::span(groups, n));
        heard.shown = false;
        radio::post(radio::UseChannel{c, channels->squelch()});
        if (view == View::Standby) {
            redrawView();
        }
    };
    // The codeplug editor: books, records, fields.
    auto plugPlaceOf = [&](Book book, std::ptrdiff_t record) {
        std::size_t place = 0;
        for (std::ptrdiff_t i = plugEditor->nearest(book, 0, 1); i >= 0 && i < record;
            i = plugEditor->nearest(book, i + 1, 1)) {
            ++place;
        }
        return place;
    };
    // The list of @p book's records, from @p record or the first in use.
    auto openPlugList = [&](Book book, std::ptrdiff_t record) {
        plugBook = book;
        plugCount = plugEditor->count(book);
        plugRecord = record >= 0 && plugEditor->inUse(book, static_cast<std::size_t>(record))
            ? record
            : plugEditor->nearest(book, 0, 1);
        plugPlace = plugRecord >= 0 ? plugPlaceOf(book, plugRecord) : 0;
        if (plugRecord < 0) {
            plugPicking = false;
            notice("None in the codeplug");
            return false;
        }
        view = View::PlugList;
        redrawView();
        return true;
    };
    auto openPlugRecord = [&](Book book, std::size_t record) {
        if (!plugEditor->open(book, record)) {
            notice("Cannot read it");
            return;
        }
        plugField = 0;
        view = View::PlugEdit;
        redrawView();
    };
    auto openPlugBook = [&](Book book) {
        if (plugEditor == nullptr) {
            notice("No codeplug editor");
            return;
        }
        plugPicking = false;
        if (plugEditor->capacity(book) == 1) {
            openPlugRecord(book, 0);
        } else {
            openPlugList(book, -1);
        }
    };
    // Leaves the open record: back to its book's list, or to the menu for a
    // record that stands alone.
    auto closePlugRecord = [&] {
        const Book book = plugEditor->book();
        const std::size_t record = plugEditor->index();
        plugEditor->close();
        if (plugEditor->capacity(book) == 1 || !openPlugList(book, static_cast<std::ptrdiff_t>(record))) {
            openGroup(MenuGroup::Codeplug, false);
        }
    };
    // Menu on a field: its value screen, the keypad, the letters, or the
    // list of records it may point to.
    auto editPlugField = [&] {
        const plug::FieldInfo f = plugEditor->field(plugField);
        if (f.readOnly) {
            notice("Read only");
            return;
        }
        plugValue = plugEditor->value(plugField);
        plugTypedLength = 0;
        switch (f.kind) {
        case plug::FieldKind::Choice:
        case plug::FieldKind::Byte:
            view = View::PlugValue;
            break;
        case plug::FieldKind::Number:
            view = (f.max - f.min) / f.step > 1000 ? View::PlugDigits : View::PlugValue;
            plugValue = std::clamp(plugValue, f.min, f.max);
            break;
        case plug::FieldKind::Frequency:
            view = View::PlugDigits;
            break;
        case plug::FieldKind::Text:
            plugEditor->show(plugField, plug::Text(plugTyped, plug::TextBytes));
            plugTypedLength = std::strlen(plugTyped);
            plugCursor = plugTypedLength;
            plugLastKey = Key::Count;
            plugTap = 0;
            plugUpper = true;
            view = View::PlugText;
            break;
        case plug::FieldKind::Link:
            plugPicking = true;
            if (!openPlugList(f.link, plugValue)) {
                return;
            }
            return;
        }
        redrawView();
    };
    // A page further on (+1) or back (-1): the first field of the next page,
    // or of this one and then the one before.
    auto plugPage = [&](int direction) {
        const std::size_t count = plugEditor->fields();
        const std::string_view page = plugEditor->field(plugField).page;
        std::size_t i = plugField;
        if (direction > 0) {
            while (i + 1 < count && plugEditor->field(i).page == page) {
                ++i;
            }
        } else {
            while (i > 0 && plugEditor->field(i - 1).page == page) {
                --i;
            }
            if (i == plugField && i > 0) {
                const std::string_view before = plugEditor->field(i - 1).page;
                while (i > 0 && plugEditor->field(i - 1).page == before) {
                    --i;
                }
            }
        }
        plugField = i;
        drawPlugEdit(false);
    };
    auto savePlugRecord = [&] {
        const Book book = plugEditor->book();
        switch (plugEditor->save()) {
        case plug::SaveResult::Saved:
            log::info("ui") << "codeplug: saved " << BookNames[static_cast<int>(book)] << ' '
                            << plugEditor->index() + 1;
            if (book >= Book::Settings) {
                notice("Saved: restarting");
                restartWanted.store(true);
                return;
            }
            if (channels != nullptr && (book == Book::Channels || book == Book::Zones)) {
                channels->reload();
                useChannel();
            }
            notice("Saved");
            closePlugRecord();
            return;
        case plug::SaveResult::Unchanged:
            closePlugRecord();
            return;
        case plug::SaveResult::Busy:
            notice("The PC is writing");
            return;
        case plug::SaveResult::Failed:
            notice("Save failed");
            return;
        }
    };
    // The keys on the codeplug editor's screens.
    auto plugKey = [&](Key key, bool press, bool step) {
        const bool digit = key >= Key::Digit0 && key <= Key::Digit9;
        const int up = key == Key::Up ? 1 : key == Key::Down ? -1
                                                             : 0;
        switch (view) {
        case View::PlugList:
            if (step && up != 0) {
                const std::ptrdiff_t next = plugEditor->nearest(plugBook, plugRecord - up, -up);
                if (next >= 0) {
                    plugRecord = next;
                    plugPlace = up < 0 ? plugPlace + 1 : plugPlace - 1;
                    drawPlugList(false);
                }
            } else if (press && key == Key::Menu && plugPicking) {
                plugPicking = false;
                plugEditor->setValue(plugField, static_cast<std::int32_t>(plugRecord));
                show(View::PlugEdit);
            } else if (press && key == Key::Hash && plugPicking && plugEditor->field(plugField).min < 0) {
                plugPicking = false;
                plugEditor->setValue(plugField, -1);
                show(View::PlugEdit);
            } else if (press && key == Key::Menu) {
                openPlugRecord(plugBook, static_cast<std::size_t>(plugRecord));
            } else if (press && key == Key::Exit) {
                if (plugPicking) {
                    plugPicking = false;
                    show(View::PlugEdit);
                } else {
                    openGroup(MenuGroup::Codeplug, false);
                }
            }
            return;
        case View::PlugEdit:
            if (step && up != 0) {
                const std::size_t count = plugEditor->fields();
                plugField = up < 0 ? (plugField + 1 < count ? plugField + 1 : plugField)
                                   : (plugField > 0 ? plugField - 1 : 0);
                drawPlugEdit(false);
            } else if (press && (key == Key::Front1 || key == Key::Front2)) {
                plugPage(key == Key::Front2 ? 1 : -1);
            } else if (press && key == Key::Menu) {
                editPlugField();
            } else if (press && key == Key::Exit) {
                if (plugEditor->changes() > 0) {
                    plugSaveRow = 0;
                    show(View::PlugSave);
                } else {
                    closePlugRecord();
                }
            }
            return;
        case View::PlugValue: {
            const plug::FieldInfo f = plugEditor->field(plugField);
            const bool buttons = f.kind == plug::FieldKind::Choice && plugSteps(f) <= 4;
            if (step && up != 0) {
                const int steps = static_cast<int>(plugSteps(f));
                const int at = static_cast<int>((plugValue - f.min) / f.step);
                const int to = ((at + (buttons ? -up : up)) % steps + steps) % steps;
                plugValue = f.min + to * f.step;
                drawPlugValue(false);
            } else if (press && key == Key::Menu) {
                plugEditor->setValue(plugField, plugValue);
                show(View::PlugEdit);
            } else if (press && key == Key::Exit) {
                show(View::PlugEdit);
            }
            return;
        }
        case View::PlugDigits: {
            const bool frequency = plugEditor->field(plugField).kind == plug::FieldKind::Frequency;
            if (press && digit && plugTypedLength < 8) {
                plugTyped[plugTypedLength++] = static_cast<char>('0' + static_cast<int>(key));
                drawPlugDigits(false);
            } else if (press && key == Key::Star && plugTypedLength > 0) {
                --plugTypedLength;
                drawPlugDigits(false);
            } else if (step && up != 0) {
                plugTypedLength = 0;
                plugValue += up * (frequency ? 12'500 : 1);
                drawPlugDigits(false);
            } else if (press && key == Key::Menu) {
                lib::StaticString<16> text;
                plugDigitsText(text);
                std::int32_t v = 0;
                for (const char c : text.view()) {
                    if (c >= '0' && c <= '9') {
                        v = v * 10 + (c - '0');
                    }
                }
                plugEditor->setValue(plugField, frequency ? v * 10 : v);
                show(View::PlugEdit);
            } else if (press && key == Key::Exit) {
                show(View::PlugEdit);
            }
            return;
        }
        case View::PlugText: {
            const std::size_t max = plugEditor->field(plugField).maxChars;
            const os::Ticks now = os::nowMs();
            if (press && digit) {
                const std::string_view set = KeyLetters[static_cast<int>(key)];
                const bool again = key == plugLastKey && now - plugLastTap < TapMs && plugCursor > 0;
                plugTap = again ? (plugTap + 1) % set.size() : 0;
                char c = set[plugTap];
                if (plugUpper && c >= 'a' && c <= 'z') {
                    c = static_cast<char>(c - 'a' + 'A');
                }
                if (again) {
                    plugTyped[plugCursor - 1] = c;
                } else if (plugCursor < max) {
                    plugTyped[plugCursor++] = c;
                    plugTypedLength = std::max(plugTypedLength, plugCursor);
                }
                plugLastKey = key;
                plugLastTap = now;
                drawPlugText(false);
                return;
            }
            plugLastKey = Key::Count;
            if (press && key == Key::Star && plugCursor > 0) {
                std::memmove(&plugTyped[plugCursor - 1], &plugTyped[plugCursor], plugTypedLength - plugCursor);
                --plugCursor;
                --plugTypedLength;
                drawPlugText(false);
            } else if (press && key == Key::Hash) {
                plugUpper = !plugUpper;
                drawPlugText(false);
            } else if (step && up != 0) {
                if (up > 0 && plugCursor > 0) {
                    --plugCursor;
                } else if (up < 0 && plugCursor < plugTypedLength && plugCursor + 1 < max) {
                    ++plugCursor;
                }
                drawPlugText(false);
            } else if (press && key == Key::Menu) {
                plugEditor->setText(plugField, std::string_view(plugTyped, plugTypedLength));
                show(View::PlugEdit);
            } else if (press && key == Key::Exit) {
                show(View::PlugEdit);
            }
            return;
        }
        case View::PlugSave:
            if (step && up != 0) {
                const std::size_t count = plugEditor->changes();
                plugSaveRow = up < 0 ? (plugSaveRow + 1 < count ? plugSaveRow + 1 : plugSaveRow)
                                     : (plugSaveRow > 0 ? plugSaveRow - 1 : 0);
                drawPlugSave(false);
            } else if (press && key == Key::Menu) {
                savePlugRecord();
            } else if (press && key == Key::Hash) {
                plugEditor->revert();
                notice("Changes dropped");
                closePlugRecord();
            } else if (press && key == Key::Exit) {
                show(View::PlugEdit);
            }
            return;
        default:
            return;
        }
    };
    // The knob on the editor's screens: rows, values and letters.
    auto plugKnob = [&](int moved) {
        if (view == View::PlugText) {
            const std::size_t max = plugEditor->field(plugField).maxChars;
            plugLastKey = Key::Count;
            if (plugCursor >= max) {
                return;
            }
            if (plugCursor == plugTypedLength) {
                plugTyped[plugTypedLength++] = ' ';
            }
            const std::size_t at = KnobChars.find(plugTyped[plugCursor]);
            const int n = static_cast<int>(KnobChars.size());
            const int to = ((at == std::string_view::npos ? 0 : static_cast<int>(at)) + moved % n + n) % n;
            plugTyped[plugCursor] = KnobChars[static_cast<std::size_t>(to)];
            drawPlugText(false);
            return;
        }
        const Key key = moved > 0 ? (view == View::PlugValue || view == View::PlugDigits ? Key::Up : Key::Down)
                                  : (view == View::PlugValue || view == View::PlugDigits ? Key::Down : Key::Up);
        for (int k = 0; k < (moved > 0 ? moved : -moved); ++k) {
            plugKey(key, false, true);
        }
    };
    auto openMenuValue = [&](MenuItem item) {
        menuItem = item;
        switch (item) {
        case MenuItem::Squelch:
            menuValue.open("Squelch");
            menuValue.add(0, "Open");
            for (std::uint8_t i = 1; i <= 5; ++i) {
                lib::StaticString<4> s;
                s << i;
                menuValue.add(i, s.view());
            }
            menuValue.select(channels->squelch());
            break;
        case MenuItem::SquelchMode:
            menuValue.open("Sq type");
            menuValue.add(0, "Carrier");
            menuValue.add(1, "Tone");
            if (channels->current()) {
                menuValue.select(channels->current()->squelchMode == SquelchMode::Tone ? 1 : 0);
            }
            break;
        case MenuItem::Power:
            menuValue.open("Power");
            menuValue.add(0, "Low");
            menuValue.add(1, "Mid");
            menuValue.add(2, "High");
            menuValue.add(3, "Turbo");
            if (channels->current()) {
                menuValue.select(static_cast<std::uint8_t>(channels->current()->power));
            }
            break;
        case MenuItem::Roaming:
            menuValue.open("Roaming");
            menuValue.add(0, "Off");
            menuValue.add(1, "On");
            menuValue.select(roamingOn ? 1 : 0);
            break;
        case MenuItem::RoamStatus:
            zonePicker.open("Roaming");
            fillRoamStatus(zonePicker);
            show(View::RoamStatus);
            return;
        case MenuItem::RoamZone: {
            menuValue.open("Roam zone");
            std::uint8_t zones[CodeplugSource::MaxRoamingZones];
            const std::size_t n = codeplug->roamingZones(zones);
            for (std::size_t i = 0; i < n; ++i) {
                std::array<char, CodeplugSource::ZoneNameBytes + 1> name{};
                codeplug->roamingZoneName(zones[i], name);
                menuValue.add(zones[i], name.data());
            }
            if (n == 0) {
                menuValue.add(services::RoamingSettings::NoZone, "None");
            }
            menuValue.select(roamingZone);
            break;
        }
        case MenuItem::FrequencyOffset:
            menuValue.open("Freq offset");
            for (int k = -MaxOffsetKhz; k <= MaxOffsetKhz; ++k) {
                lib::StaticString<12> s;
                s << (k > 0 ? "+" : "") << k << " kHz";
                menuValue.add(static_cast<std::uint8_t>(k + MaxOffsetKhz), s.view());
            }
            menuValue.select(static_cast<std::uint8_t>(offsetKhz + MaxOffsetKhz));
            break;
        case MenuItem::Backlight:
            menuValue.open("Backlight");
            for (std::uint8_t i = 1; i <= Display::BacklightLevels; ++i) {
                lib::StaticString<4> s;
                s << i;
                menuValue.add(i, s.view());
            }
            menuValue.select(static_cast<std::uint8_t>(backlightLevel));
            break;
        case MenuItem::BacklightTime:
            menuValue.open("Light time");
            for (std::uint8_t i = 0; i < std::size(BacklightTimes); ++i) {
                menuValue.add(i, durationText(BacklightTimes[i]).view());
                if (BacklightTimes[i] == backlightS) {
                    menuValue.select(i);
                }
            }
            break;
        case MenuItem::BigName:
            menuValue.open("Big name");
            menuValue.add(0, "Channel");
            menuValue.add(1, "Contact");
            menuValue.select(bigContactName ? 1 : 0);
            break;
        case MenuItem::BesidePower:
            menuValue.open("Next to power");
            menuValue.add(0, "Voltage");
            menuValue.add(1, "Volume");
            menuValue.select(volumeBesidePower ? 1 : 0);
            break;
        case MenuItem::BatterySave:
            menuValue.open("Battery save");
            menuValue.add(0, "Off");
            menuValue.add(1, "On", "150 ms");
            menuValue.add(2, "Fast wake", "70 ms");
            menuValue.select(batterySave);
            break;
        case MenuItem::KeyLock:
            menuValue.open("Lock covers");
            menuValue.add(0, "All");
            menuValue.add(1, "Keys+PTT");
            menuValue.add(2, "Keypad");
            menuValue.add(3, "Side+PTT");
            break;
        case MenuItem::AutoLock:
            menuValue.open("Auto lock");
            menuValue.add(0, "Off");
            menuValue.add(10, "10 s");
            menuValue.add(30, "30 s");
            menuValue.add(60, "60 s");
            menuValue.select(static_cast<std::uint8_t>(keyLock.settings().autoLockMs / 1000));
            break;
        case MenuItem::Gps:
            menuValue.open("GPS");
            menuValue.add(0, "Off");
            menuValue.add(1, "On");
            menuValue.select(gpsState.on ? 1 : 0);
            break;
        case MenuItem::GpsSystems: {
            namespace g = services::gps_systems;
            menuValue.open("Systems");
            menuValue.add(g::Gps | g::Beidou, "GPS + BeiDou");
            menuValue.add(g::Gps, "GPS");
            menuValue.add(g::Beidou, "BeiDou");
            menuValue.add(g::Gps | g::Glonass, "GPS + GLONASS");
            menuValue.add(g::Glonass, "GLONASS");
            menuValue.add(g::Beidou | g::Glonass, "BeiDou + GLONASS");
            menuValue.add(g::All, "All three");
            menuValue.select(gpsSystems);
            break;
        }
        case MenuItem::ClockZone:
            menuValue.open("Zone");
            for (int i = 0; i < ZoneSteps; ++i) {
                menuValue.add(static_cast<std::uint8_t>(i), zoneText((i - 24) * 30).view());
            }
            menuValue.select(static_cast<std::uint8_t>(timeZoneMinutes / 30 + 24));
            break;
        case MenuItem::ClockShown:
            menuValue.open("Show time");
            menuValue.add(0, "Local");
            menuValue.add(1, "UTC");
            menuValue.select(clockUtc ? 1 : 0);
            break;
        case MenuItem::ClockCalibration: {
            menuValue.open("Calibrate");
            menuValue.add(60, "60 min", "GPS on");
            menuValue.add(30, "30 min", "GPS on");
            if (clockCalibration.state == ClockCalibration::State::Running) {
                menuValue.add(0, "Stop");
            }
            // The correction in effect, for information only.
            const std::int32_t rate = system::clockRate();
            menuValue.add(CalibrationInfo, "Correction", rate == 0 ? std::string_view("None") : system::ppmText(rate).view());
            menuValue.add(CalibrationInfo + 1, "Per day", system::perDayText(rate).view());
            menuValue.select(clockCalibration.state == ClockCalibration::State::Running ? 0 : 60);
            break;
        }
        case MenuItem::GpsStatus:
            show(View::Gps);
            return;
        case MenuItem::AprsBeacon:
            menuValue.open("Beacon");
            for (std::uint8_t i = 0; i < std::size(BeaconIntervals); ++i) {
                menuValue.add(i, BeaconIntervals[i] == 0 ? std::string_view("Off") : durationText(BeaconIntervals[i]).view());
                if (BeaconIntervals[i] == beaconIntervalS) {
                    menuValue.select(i);
                }
            }
            break;
        case MenuItem::AprsPower: {
            constexpr const char *Powers[] = {"Low", "Mid", "High", "Turbo"};
            menuValue.open("Beacon power");
            for (std::uint8_t i = 0; i < 4; ++i) {
                menuValue.add(i, Powers[i]);
            }
            menuValue.select(static_cast<std::uint8_t>(beaconPower));
            break;
        }
        case MenuItem::AprsSend:
            aprs::post(aprs::BeaconNow{});
            notice("Beacon queued");
            return;
        case MenuItem::PlugChannels:
        case MenuItem::PlugZones:
        case MenuItem::PlugContacts:
        case MenuItem::PlugGroups:
        case MenuItem::PlugRepeaters:
        case MenuItem::PlugRoamZones:
        case MenuItem::PlugRadioIds:
        case MenuItem::PlugSettings:
        case MenuItem::PlugMoreSettings:
        case MenuItem::PlugAprs:
        case MenuItem::PlugAprsText:
            openPlugBook(static_cast<Book>(static_cast<int>(item) - static_cast<int>(MenuItem::PlugChannels)));
            return;
        case MenuItem::ToolLog:
            show(View::Log);
            return;
        case MenuItem::ToolKeys:
            show(View::KeyTest);
            return;
        case MenuItem::Version:
            return;
        case MenuItem::AprsReceive:
            menuValue.open("Receive");
            menuValue.add(0, "Off");
            menuValue.add(1, "On");
            menuValue.select(aprsReceive ? 1 : 0);
            break;
        case MenuItem::Count:
            return;
        }
        {
            const char *label = "";
            lib::StaticString<12> value;
            itemRow(item, label, value);
            lib::StaticString<ListPicker::TitleBytes> title;
            title << Groups[static_cast<int>(menuGroup)].name << " > " << label;
            menuValue.retitle(title.view());
        }
        menuValueInUse = menuValue.cursor();
        view = View::MenuValue;
        redrawView();
    };
    // Redraws whatever shows the calibration: the GPS group or the
    // Calibrate list, keeping the cursor.
    auto refreshCalibration = [&] {
        refreshGpsGroup();
        if (view == View::MenuValue && menuItem == MenuItem::ClockCalibration) {
            const std::uint8_t cursor = menuValue.chosen();
            openMenuValue(MenuItem::ClockCalibration);
            menuValue.select(cursor);
            redrawView();
        }
    };
    auto tuneEntry = [&] {
        std::uint32_t khz = 0;
        for (std::size_t i = 0; i < EntryDigits; ++i) {
            khz = khz * 10 + static_cast<std::uint32_t>(i < entryLength ? entry[i] - '0' : 0);
        }
        entryLength = 0;
        channels->tune(khz * 1000);
        log::info("ui") << "VFO " << khz / 1000 << "." << khz % 1000 / 100 << khz % 100 / 10 << khz % 10 << " MHz";
        useChannel();
        redrawView();
    };
    auto openZonePicker = [&] {
        static std::uint8_t used[CodeplugSource::MaxZones];
        zonePicker.open("Zone");
        const std::size_t n = channels->zones(used);
        for (std::size_t i = 0; i < n; ++i) {
            char name[ChannelSelector::NameBytes];
            channels->zoneLabel(used[i], name);
            zonePicker.add(used[i], name);
        }
        zonePicker.select(channels->zone());
        show(View::ZonePicker);
    };
    // The value chosen in the menu takes effect at once, for this session.
    auto applyMenuValue = [&] {
        const std::uint8_t v = menuValue.chosen();
        switch (menuItem) {
        case MenuItem::Squelch:
            channels->setSquelch(v);
            log::info("ui") << "squelch " << v;
            break;
        case MenuItem::SquelchMode:
            channels->setSquelchMode(v == 1 ? SquelchMode::Tone : SquelchMode::Carrier);
            log::info("ui") << "squelch opens on " << (v == 1 ? "the tone" : "the carrier");
            break;
        case MenuItem::Power:
            channels->setPower(static_cast<PowerLevel>(v));
            log::info("ui") << "power " << v;
            break;
        case MenuItem::Roaming:
            roamingOn = v == 1;
            radio::post(radio::RoamingOn{roamingOn});
            break;
        case MenuItem::RoamStatus:
            break;
        case MenuItem::RoamZone:
            if (v != roamingZone) {
                roamingZone = v;
                loadRoamingSites();
                log::info("ui") << "roaming zone " << v << ", " << roamingSites << " repeaters";
            }
            break;
        case MenuItem::FrequencyOffset:
            offsetKhz = static_cast<int>(v) - MaxOffsetKhz;
            radio::post(radio::FrequencyOffset{offsetKhz * 1000});
            break;
        case MenuItem::Backlight:
            backlightLevel = v;
            d.setBacklight(v);
            break;
        case MenuItem::BacklightTime:
            backlightS = BacklightTimes[v < std::size(BacklightTimes) ? v : 0];
            log::info("ui") << "light time " << durationText(backlightS).view();
            break;
        case MenuItem::BigName:
            bigContactName = v == 1;
            break;
        case MenuItem::BesidePower:
            volumeBesidePower = v == 1;
            break;
        case MenuItem::BatterySave:
            setBatterySave(v);
            break;
        case MenuItem::KeyLock: {
            services::KeyLockSettings s = keyLock.settings();
            s.keypad = v != 3;
            s.side = v != 2;
            s.knob = v == 0;
            keyLock.setSettings(s);
            logLockSettings();
            break;
        }
        case MenuItem::AutoLock: {
            services::KeyLockSettings s = keyLock.settings();
            s.autoLockMs = static_cast<std::uint32_t>(v) * 1000;
            keyLock.setSettings(s);
            logLockSettings();
            break;
        }
        case MenuItem::Gps:
            // The menu shows the new value before the gps task reports it.
            gpsState.on = v == 1;
            gps::post(gps::Power{v == 1});
            break;
        case MenuItem::GpsSystems:
            gpsSystems = v;
            gps::post(gps::Systems{v});
            break;
        case MenuItem::ClockZone:
            timeZoneMinutes = (v - 24) * 30;
            log::info("ui") << "time zone " << zoneText(timeZoneMinutes).view();
            break;
        case MenuItem::ClockShown:
            clockUtc = v == 1;
            break;
        case MenuItem::ClockCalibration:
            if (v < CalibrationInfo) {
                gps::post(gps::CalibrateClock{v});
            }
            break;
        case MenuItem::AprsBeacon:
            beaconIntervalS = BeaconIntervals[v < std::size(BeaconIntervals) ? v : 0];
            aprs::post(aprs::BeaconInterval{beaconIntervalS});
            refreshLockMark();
            break;
        case MenuItem::AprsPower:
            beaconPower = static_cast<PowerLevel>(v & 3);
            aprs::post(aprs::BeaconPower{beaconPower});
            break;
        case MenuItem::AprsReceive:
            aprsReceive = v == 1;
            aprs::post(aprs::Receive{aprsReceive});
            break;
        case MenuItem::GpsStatus:
        case MenuItem::AprsSend:
        case MenuItem::PlugChannels:
        case MenuItem::PlugZones:
        case MenuItem::PlugContacts:
        case MenuItem::PlugGroups:
        case MenuItem::PlugRepeaters:
        case MenuItem::PlugRoamZones:
        case MenuItem::PlugRadioIds:
        case MenuItem::PlugSettings:
        case MenuItem::PlugMoreSettings:
        case MenuItem::PlugAprs:
        case MenuItem::PlugAprsText:
        case MenuItem::ToolLog:
        case MenuItem::ToolKeys:
        case MenuItem::Version:
        case MenuItem::Count:
            break;
        }
        if (menuItem == MenuItem::Squelch || menuItem == MenuItem::SquelchMode || menuItem == MenuItem::Power ||
            menuItem == MenuItem::Roaming || menuItem == MenuItem::RoamZone) {
            useChannel();
        }
        // Back to the list it came from, showing the new value; straight to
        // the standby screen when opened from there.
        if (menuQuick) {
            show(View::Standby);
        } else {
            openGroup(menuGroup, false);
        }
    };
    // A key on the standby screen or the zone list, after the lock let it
    // through.
    auto channelKey = [&](Key key, KeyDebouncer::EventType type) {
        using Type = KeyDebouncer::EventType;
        const bool press = type == Type::Press;
        const bool step = press || type == Type::Repeat;
        if (view >= View::PlugList && view <= View::PlugSave) {
            plugKey(key, press, step);
            return;
        }
        if (view == View::Menu || view == View::MenuValue) {
            if (step && (key == Key::Up || key == Key::Down)) {
                if (view == View::Menu) {
                    moveMenu(key == Key::Up ? -1 : 1);
                } else {
                    // On the spinner Up raises the value; buttons and lists
                    // move the way they are drawn.
                    const int up = menuValueSpinner() ? 1 : -1;
                    menuValue.move(key == Key::Up ? up : -up);
                    drawMenuValue(false);
                }
            } else if (press && key == Key::Menu) {
                if (view == View::MenuValue) {
                    applyMenuValue();
                } else if (menuAtGroups) {
                    openGroup(static_cast<MenuGroup>(menuRow), false);
                } else {
                    openMenuValue(Groups[static_cast<int>(menuGroup)].items[menuRow]);
                }
            } else if (press && key == Key::Exit) {
                if (view == View::MenuValue) {
                    openGroup(menuGroup, menuQuick);
                } else if (!menuAtGroups && !menuQuick) {
                    openMenu();
                } else {
                    show(View::Standby);
                }
            }
            return;
        }
        if (view == View::RoamStatus) {
            if (step && (key == Key::Up || key == Key::Down)) {
                zonePicker.move(key == Key::Up ? -1 : 1);
                zonePicker.draw(d, StandbyScreen::StatusH, listHint());
            } else if (press && key == Key::Menu && zonePicker.chosen() == SearchNowRow) {
                radio::post(radio::RoamNow{});
            } else if (press && key == Key::Exit) {
                openGroup(menuGroup, menuQuick);
            }
            return;
        }
        if (view == View::Activity) {
            if (step && (key == Key::Up || key == Key::Down)) {
                openLogPage(logPage + (key == Key::Up ? -1 : 1));
            } else if (press && (key == Key::Front2 || key == Key::Exit || key == Key::Menu)) {
                show(View::Standby);
            }
            return;
        }
        if (view == View::ZonePicker) {
            if (step && (key == Key::Up || key == Key::Down)) {
                zonePicker.move(key == Key::Up ? -1 : 1);
                zonePicker.draw(d, StandbyScreen::StatusH, listHint());
            } else if (press && (key == Key::Menu || key == Key::Front1)) {
                channels->selectZone(zonePicker.chosen());
                show(View::Standby);
                useChannel();
            } else if (press && key == Key::Exit) {
                show(View::Standby);
            }
            return;
        }
        // A key on the call screen goes back to standby; Exit does only that.
        if (press && heard.shown) {
            heard.shown = false;
            redrawView();
            if (key == Key::Exit) {
                return;
            }
        }
        if (step && (key == Key::Up || key == Key::Down)) {
            entryLength = 0;
            const int dir = key == Key::Up ? 1 : -1;
            if (channels->vfoMode()) {
                channels->tune(channels->current()->receiveHz + static_cast<std::uint32_t>(dir * VfoStepHz));
            } else {
                channels->step(dir);
            }
            useChannel();
            redrawView();
        } else if (press && key >= Key::Digit0 && key <= Key::Digit9) {
            if (entryLength < EntryDigits) {
                entry[entryLength++] = static_cast<char>('0' + static_cast<int>(key) - static_cast<int>(Key::Digit0));
            }
            if (entryLength == EntryDigits) {
                tuneEntry();
            } else {
                redrawView();
            }
        } else if (press && key == Key::Menu && entryLength > 0) {
            tuneEntry();
        } else if (press && key == Key::Menu) {
            openMenu();
        } else if (press && key == Key::Exit && entryLength > 0) {
            entryLength = 0;
            redrawView();
        } else if (press && key == Key::Hash) {
            entryLength = 0;
            if (channels->vfoMode()) {
                channels->memoryMode();
            } else {
                channels->tune(channels->current()->receiveHz);
            }
            useChannel();
            redrawView();
        } else if (press && key == Key::Front1) {
            entryLength = 0;
            openZonePicker();
        } else if (press && key == Key::Front2) {
            entryLength = 0;
            openLogPage(logPage);
        }
    };
    // A side key's function: on the press when it has no long-press
    // function, else on the release before a long press.
    auto sideKey = [&](Key key, KeyDebouncer::EventType type) {
        using Type = KeyDebouncer::EventType;
        const std::size_t k = static_cast<std::size_t>(key) - static_cast<std::size_t>(Key::Side1);
        services::KeyFunction f = services::KeyFunction::None;
        if (type == Type::Press) {
            sideLongDone[k] = false;
            f = sideLong[k] == services::KeyFunction::None ? sideShort[k] : services::KeyFunction::None;
        } else if (type == Type::LongPress) {
            sideLongDone[k] = true;
            f = sideLong[k];
        } else if (type == Type::Release && sideLong[k] != services::KeyFunction::None && !sideLongDone[k]) {
            f = sideShort[k];
        }
        if (f == services::KeyFunction::Roam) {
            radio::post(radio::RoamNow{});
        }
    };
    auto onLockVerdict = [&](Verdict v) {
        if (v == Verdict::Locked || v == Verdict::Unlocked) {
            refreshLockMark();
        }
        switch (v) {
        case Verdict::Pass:
        case Verdict::Ignored:
            break;
        case Verdict::Refused:
            audio::post(audio::Beep{audio::Sound::Refused});
            notice("Keys locked");
            break;
        case Verdict::Locked:
            audio::post(audio::Beep{audio::Sound::Locked});
            log::info("ui") << "keys locked";
            notice("Keys locked");
            break;
        case Verdict::UnlockPrompt:
            notice("* to unlock");
            break;
        case Verdict::Unlocked:
            audio::post(audio::Beep{audio::Sound::Unlocked});
            log::info("ui") << "keys unlocked";
            notice("Keys unlocked");
            break;
        }
    };

    while (!os::stopRequested()) {
        Message m;
        if (inbox.receive(m, RedrawStepMs)) {
            auto refreshValues = [&] {
                if (view == View::KeyTest) {
                    keyTest.drawValues(d);
                }
            };
            lib::visit(lib::Overload{
                           [&](const Backlight &b) {
                               backlightLevel = b.level;
                               d.setBacklight(b.level);
                               backlightLit = true;
                               lastLightUse = os::nowMs();
                               services::power::hold(services::power::AwakeReason::Display);
                               log::info("ui") << "backlight " << b.level;
                           },
                           [&](const BacklightTime &t) {
                               backlightS = t.seconds;
                               useLight(d);
                               log::info("ui") << "light time " << durationText(backlightS).view();
                           },
                           [&](const ShowTestPattern &) {
                               drawTestPattern(d);
                               view = View::TestPattern;
                           },
                           [&](const ShowLog &) { show(View::Log); },
                           [&](const ShowKeyTest &) { show(View::KeyTest); },
                           [&](const ToggleView &) { show(nextView()); },
                           [&](const PanelPower &p) { d.setPanelOn(p.on); },
                           [&](const Keys &k) { keyTest.setKeys(d, k.mask, view == View::KeyTest); },
                           [&](const KeyEvent &e) {
                               using Type = KeyDebouncer::EventType;
                               useLight(d);
                               // Until the radio's chips are up, at start-up or while one
                               // restarts, every key is refused: nothing may reach them
                               // half set up. A PTT release still goes through.
                               if (!hardwareUp() && !(e.key == Key::Ptt && e.type == Type::Release)) {
                                   refuseNotReady(e.type == Type::Press);
                                   return;
                               }
                               if (e.key == Key::Ptt) {
                                   // Press and release only; a refused press never keys
                                   // the radio, a release always reaches it.
                                   if (e.type == Type::Press || e.type == Type::Release) {
                                       const bool down = e.type == Type::Press;
                                       const Verdict v = keyLock.ptt(down, lockContext());
                                       if (v == Verdict::Pass) {
                                           radio::post(radio::Ptt{down});
                                       } else {
                                           log::info("ui") << "PTT refused: keys locked";
                                           onLockVerdict(v);
                                       }
                                   }
                                   return;
                               }
                               const Verdict v = keyLock.key(e.key, e.type, lockContext());
                               if (v != Verdict::Pass) {
                                   // Repeats of a refused key stay quiet.
                                   if (e.type != Type::Repeat) {
                                       onLockVerdict(v);
                                   }
                                   return;
                               }
                               if (e.type == Type::Press) {
                                   audio::post(audio::Beep{audio::Sound::Key});
                               }
                               if (e.key == Key::Exit && e.type == Type::LongPress) {
                                   entryLength = 0;
                                   show(nextView());
                                   return;
                               }
                               if (view == View::Gps) {
                                   if (e.type == Type::Press && (e.key == Key::Exit || e.key == Key::Menu)) {
                                       openGroup(MenuGroup::Position, false);
                                   }
                                   return;
                               }
                               const bool step = e.type == Type::Press || e.type == Type::Repeat;
                               // The APRS page under P2: the knob picks a station,
                               // Menu opens it, Up/Down change the page.
                               if (view == View::Heard) {
                                   if (step && (e.key == Key::Up || e.key == Key::Down)) {
                                       openLogPage(logPage + (e.key == Key::Up ? -1 : 1));
                                   } else if (e.type == Type::Press && e.key == Key::Menu && stationShown < heardShownCount) {
                                       show(View::Station);
                                   } else if (e.type == Type::Press && (e.key == Key::Exit || e.key == Key::Front2)) {
                                       show(View::Standby);
                                   }
                                   return;
                               }
                               if (view == View::Station) {
                                   if (step && e.key == Key::Up && stationShown > 0) {
                                       --stationShown;
                                       redrawView();
                                   } else if (step && e.key == Key::Down && stationShown + 1 < heardShownCount) {
                                       ++stationShown;
                                       redrawView();
                                   } else if (e.type == Type::Press && (e.key == Key::Exit || e.key == Key::Menu)) {
                                       reopenHeard();
                                   }
                                   return;
                               }
                               if (e.key >= Key::Side1 && e.key <= Key::Side3) {
                                   if (channels != nullptr) {
                                       sideKey(e.key, e.type);
                                   }
                                   return;
                               }
                               if (channels != nullptr && (view == View::Standby || isList(view) || view == View::Activity)) {
                                   channelKey(e.key, e.type);
                               }
                           },
                           [&](const Transmitting &t) {
                               const bool wasCall = transmitting && transmitCall;
                               transmitting = t.on;
                               using Kind = services::EventKind;
                               if (t.on && t.call) {
                                   transmitCall = true;
                                   transmitStarted = os::nowMs();
                                   const std::optional<Channel> ch =
                                       channels != nullptr ? channels->current() : std::nullopt;
                                   if (ch && ch->digital()) {
                                       logEvent(Kind::CallOut, 0, ch->dmr.contact.id, 0,
                                           ch->dmr.contact.type == services::dmr::CallType::Group);
                                   } else if (ch) {
                                       logEvent(Kind::CallOut, 1, ch->transmitHz / 1000);
                                   }
                               } else if (!t.on && wasCall) {
                                   transmitCall = false;
                                   callLog.update(Kind::CallOut,
                                       static_cast<unsigned>((os::nowMs() - transmitStarted) / 1000));
                                   if (view == View::Activity && !noticeShown) {
                                       drawActivity();
                                   }
                               }
                               useLight(d);
                               if (view == View::Log) {
                                   drawHeader(d);
                               }
                               // A call takes the screen and gives it back.
                               if (view == View::Standby && !noticeShown && (t.call || wasCall)) {
                                   redrawView();
                               } else {
                                   refreshLockMark();
                               }
                           },
                           [&](const Receiving &r) {
                               receiving = r.on;
                               useLight(d);
                               refreshLockMark();
                           },
                           [&](const DmrCall &c) {
                               if (c.active && !heard.active) {
                                   logEvent(services::EventKind::CallIn, c.source, c.destination, 0, c.group);
                               } else if (!c.active && heard.active) {
                                   callLog.update(services::EventKind::CallIn,
                                       static_cast<unsigned>((os::nowMs() - heard.started) / 1000));
                                   if (view == View::Activity && !noticeShown) {
                                       drawActivity();
                                   }
                               }
                               if (c.active) {
                                   heard.shown = true;
                                   heard.active = true;
                                   heard.group = c.group;
                                   heard.source = c.source;
                                   heard.destination = c.destination;
                                   heard.started = os::nowMs();
                                   nameCall(heard, codeplug);
                                   lastLightUse = os::nowMs();
                               } else if (heard.active) {
                                   heard.active = false;
                                   heard.ended = os::nowMs();
                               }
                               if (view == View::Standby && !noticeShown) {
                                   redrawView();
                               }
                           },
                           [&](const Roaming &r) {
                               // A move to another repeater is logged once.
                               if (!r.searching && r.site >= 0 && r.site != roamLogged) {
                                   roamLogged = r.site;
                                   logEvent(services::EventKind::RoamMove, static_cast<std::uint32_t>(r.site) + 1);
                               }
                               roam.searching = r.searching;
                               roam.site = r.site;
                               roam.receiveHz = r.receiveHz;
                               roam.colourCode = r.colourCode;
                               roam.name[0] = '\0';
                               std::array<char, CodeplugSource::SiteNameBytes + 1> name{};
                               if (r.site >= 0 && codeplug != nullptr &&
                                   codeplug->roamingSiteName(roamingZone, static_cast<std::size_t>(r.site), name)) {
                                   copyName(roam.name, name.data());
                               }
                               if (view == View::Standby && !heard.shown && !noticeShown) {
                                   redrawView();
                               }
                           },
                           [&](const Notice &n) {
                               if (view != View::Start) {
                                   notice(n.text);
                               }
                           },
                           [&](const ShowStandby &) {
                               if (channels != nullptr) {
                                   show(View::Standby);
                               }
                           },
                           [&](const SetKeyLock &s) {
                               keyLock.setLocked(s.locked);
                               refreshLockMark();
                               logLockSettings();
                               notice(s.locked ? "Keys locked" : "Keys unlocked");
                           },
                           [&](const BatterySave &b) { setBatterySave(b.mode); },
                           [&](const KeyLockSettings &s) {
                               keyLock.setSettings(s.settings);
                               refreshLockMark();
                               logLockSettings();
                           },
                           [&](const KeyLockReport &) { logLockSettings(); },
                           [&](const EncoderClicks &c) {
                               useLight(d);
                               if (!hardwareUp()) {
                                   refuseNotReady(true);
                                   return;
                               }
                               const Verdict v = keyLock.knob(lockContext());
                               if (v != Verdict::Pass) {
                                   onLockVerdict(v);
                                   return;
                               }
                               static std::int32_t lastClicks = 0;
                               const std::int32_t moved = c.clicks - lastClicks;
                               lastClicks = c.clicks;
                               keyTest.setClicks(c.clicks);
                               refreshValues();
                               if (channels != nullptr && moved != 0) {
                                   if (view >= View::PlugList && view <= View::PlugSave) {
                                       plugKnob(static_cast<int>(moved));
                                   } else if (view == View::Menu) {
                                       moveMenu(moved);
                                   } else if (view == View::MenuValue) {
                                       menuValue.move(moved);
                                       drawMenuValue(false);
                                   } else if (isList(view)) {
                                       ListPicker &list = listOf(view);
                                       list.move(moved);
                                       list.draw(d, StandbyScreen::StatusH, listHint());
                                   } else if (view == View::Heard && heardShownCount > 0) {
                                       const int last = static_cast<int>(heardShownCount) - 1;
                                       stationShown = static_cast<std::size_t>(
                                           std::clamp(static_cast<int>(stationShown) + static_cast<int>(moved), 0, last));
                                       drawHeard();
                                   } else if (view == View::Standby) {
                                       entryLength = 0;
                                       if (channels->vfoMode()) {
                                           channels->tune(channels->current()->receiveHz +
                                               static_cast<std::uint32_t>(moved * static_cast<int>(VfoStepHz)));
                                       } else {
                                           channels->step(moved);
                                       }
                                       useChannel();
                                   }
                               }
                           },
                           [&](const EncoderRaw &r) {
                               keyTest.setRaw(r.raw);
                               refreshValues();
                           },
                           [&](const Volume &v) {
                               keyTest.setVolume(v.permille);
                               refreshValues();
                               audio::post(audio::VolumeKnob{v.permille});
                               const int step = services::volumeStep(v.permille);
                               const bool changed = step != volumeLevel;
                               if (changed && volumeLevel >= 0) {
                                   useLight(d);
                               }
                               volumeLevel = step;
                               if (changed && view == View::Standby) {
                                   // Only what changes: the volume next to the
                                   // power, and the notice's number and bar once
                                   // it is up.
                                   if (workingScreen() && volumeBesidePower) {
                                       standby.drawReading(d, standbyState());
                                   }
                                   drawVolumeNotice(d, step, !(noticeShown && volumeNoticeUp));
                                   noticeShown = true;
                                   volumeNoticeUp = true;
                                   noticeUntil = os::nowMs() + NoticeMs;
                               }
                           },
                           [&](const Battery &b) {
                               batteryState = {b.millivolts, b.level, b.low};
                               if (view != View::Start && view != View::TestPattern && view != View::Message) {
                                   drawBattery(d);
                               }
                               if (workingScreen() && !volumeBesidePower && !noticeShown) {
                                   standby.drawReading(d, standbyState());
                               }
                           },
                           [&](const Gps &g) {
                               const bool tagChanged = g.on != gpsState.on || g.receiving != gpsState.receiving ||
                                   g.fix.valid != gpsState.fix.valid || g.asleep != gpsState.asleep;
                               // A fix is logged when it comes after a time without
                               // one, not at every wake-up between fixes.
                               if (g.fix.valid && !gpsState.fix.valid && !g.asleep &&
                                   (fixLoggedS == 0 || uptimeS() - fixLoggedS >= 600)) {
                                   fixLoggedS = uptimeS();
                                   logEvent(services::EventKind::GpsFix);
                               }
                               gpsState = {g.on, g.receiving, g.fix, g.asleep};
                               if (view == View::Gps) {
                                   if (tagChanged) {
                                       standby.drawStatus(d, standbyState());
                                       drawBattery(d);
                                   }
                                   if (!noticeShown) {
                                       drawGpsScreen(d, gpsState, false);
                                   }
                               } else if (tagChanged) {
                                   refreshLockMark();
                                   refreshGpsGroup();
                               }
                           },
                           [&](const ClockRate &) { refreshCalibration(); },
                           [&](const PlugCommand &) {
                               if (!plugEditor->debugCommand(plugCommandText.view())) {
                                   log::info("console") << "unknown command";
                               }
                               plugCommandWaiting.store(false);
                           },
                           [&](const ClockCalibration &c) {
                               clockCalibration = c;
                               refreshCalibration();
                               switch (c.state) {
                               case ClockCalibration::State::Done:
                                   notice("Clock calibrated");
                                   break;
                               case ClockCalibration::State::TooFewFixes:
                                   notice("Too few fixes");
                                   break;
                               case ClockCalibration::State::GpsOff:
                                   notice("Switch GPS on");
                                   break;
                               case ClockCalibration::State::Running:
                               case ClockCalibration::State::Stopped:
                                   break;
                               }
                           },
                           [&](const Packet &p) {
                               if (p.sent) {
                                   logEvent(services::EventKind::AprsBeacon);
                               }
                               notice(p.sent ? "Beacon sent" : "Beacon not sent");
                           },
                           [&](const AprsHeard &) {
                               if (view == View::Heard && !noticeShown) {
                                   reopenHeard();
                               }
                           },
                           [&](const AprsProblem &p) {
                               notice(p.kind == AprsProblem::Kind::NoCallsign     ? "No APRS callsign"
                                       : p.kind == AprsProblem::Kind::NoFrequency ? "No APRS frequency"
                                       : p.kind == AprsProblem::Kind::NoFix       ? "No GPS position"
                                                                                  : "Radio busy");
                           },
                           [&](const SwitchingOff &) {
                               view = View::Message;
                               d.fillRect(0, 0, d.width(), d.height(), colors::Black);
                               drawText(d, fonts::Spleen8x16, (d.width() - 14 * 8) / 2, (d.height() - 16) / 2, "Switching off",
                                   colors::White, colors::Black);
                           },
                       },
                m);
        }
        if (keyLock.update(lockContext())) {
            audio::post(audio::Beep{audio::Sound::Locked});
            log::info("ui") << "keys locked (auto lock)";
            refreshLockMark();
            notice("Keys locked");
        }
        if (noticeShown && static_cast<std::int32_t>(os::nowMs() - noticeUntil) >= 0) {
            noticeShown = false;
            volumeNoticeUp = false;
            redrawView();
        }
        // The start screen stays until the radio is ready, and at least its
        // own time.
        const bool startShown = static_cast<std::int32_t>(os::nowMs() - startUntil) >= 0;
        if (view == View::Start && startShown && !radio::ready() && !startingShown) {
            startingShown = true;
            drawStarting(d);
        }
        if (view == View::Start && startShown && radio::ready()) {
            show(channels != nullptr ? View::Standby : View::KeyTest);
            setBatterySave(batterySave);
            gps::post(gps::Systems{gpsSystems});
            if (gpsAtStart) {
                gps::post(gps::Power{true});
            }
            aprs::post(aprs::Receive{aprsReceive});
            useChannel();
        }
        // The backlight goes off after its time, never during a signal or
        // a transmission.
        if (backlightLit && backlightS != 0 && !transmitting && !receiving && view != View::Message &&
            os::nowMs() - lastLightUse >= static_cast<os::Ticks>(backlightS) * 1000) {
            backlightLit = false;
            d.setBacklight(0);
            services::power::release(services::power::AwakeReason::Display);
        }
        // The GPS screen wants a position every second while open.
        if ((view == View::Gps) != gpsScreenNeed) {
            gpsScreenNeed = view == View::Gps;
            gps::post(gps::Need{services::GpsSchedule::Client::Screen, gpsScreenNeed ? 1000u : 0u});
        }
        // Ages on the heard screens move on.
        if ((view == View::Heard || view == View::Station) && !noticeShown && os::nowMs() - lastHeardRefresh >= HeardRefreshMs) {
            lastHeardRefresh = os::nowMs();
            if (view == View::Station) {
                redrawView();
            } else {
                reopenHeard();
            }
        }
        // The status bar's clock moves on each minute.
        if (const auto utc = system::utcNow()) {
            const os::Ticks minute = *utc / 60;
            if (minute != lastClockMinute) {
                lastClockMinute = minute;
                if (!noticeShown && view == View::Standby && !heard.shown && !(transmitting && transmitCall)) {
                    standby.drawClock(d, standbyState());
                }
            }
        }
        // The transmit screen: its time each second.
        if (view == View::Standby && transmitting && transmitCall && !noticeShown) {
            const os::Ticks second = os::nowMs() / 1000;
            if (second != lastSecond) {
                lastSecond = second;
                standby.drawTransmitTime(d, transmitInfo());
            }
        }
        // The call screen: its time each second, back to standby a moment
        // after the call.
        if (view == View::Standby && heard.shown && !noticeShown) {
            const os::Ticks second = os::nowMs() / 1000;
            if (!heard.active && os::nowMs() - heard.ended >= CallHoldMs) {
                heard.shown = false;
                redrawView();
            } else if (heard.active && second != lastSecond) {
                lastSecond = second;
                standby.drawCallTime(d, callInfo());
            }
        }
        if (view == View::RoamStatus && !noticeShown) {
            const os::Ticks second = os::nowMs() / 1000;
            if (second != lastSecond) {
                lastSecond = second;
                fillRoamStatus(zonePicker);
                zonePicker.draw(d, StandbyScreen::StatusH, listHint());
            }
        }
        if (view == View::Log) {
            const os::Ticks second = os::nowMs() / 1000;
            if (second != lastSecond) {
                lastSecond = second;
                drawHeader(d);
            }
            if (!noticeShown) {
                logView.draw(d, HeaderFont.height, false);
            }
        }
    }
}

} // namespace

void start(Display &d, CodeplugSource *c, const board::Input *i, board::PlugEditor *e)
{
    display = &d;
    codeplug = c;
    input = i;
    plugEditor = e;
    log::setTap(onLogLine);
    uiTask.start("ui", 4, uiMain, nullptr);
}

bool post(const Message &message)
{
    if (display == nullptr) {
        return false;
    }
    return inbox.send(message);
}

bool takeRestart()
{
    return restartWanted.exchange(false);
}

bool plugCommand(std::string_view text)
{
    if (plugEditor == nullptr || plugCommandWaiting.load()) {
        return false;
    }
    plugCommandText.clear();
    plugCommandText << text;
    plugCommandWaiting.store(true);
    if (!post(PlugCommand{})) {
        plugCommandWaiting.store(false);
        return false;
    }
    return true;
}

} // namespace dmm::app::ui
