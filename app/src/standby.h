// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tommi Tauriainen

#pragma once

#include "aprs.h"

#include "dmm/services/channel_selector.h"
#include "dmm/services/display.h"
#include "dmm/services/nmea.h"

#include <cstdint>
#include <string_view>

namespace dmm::app::ui {

/// The palette of the radio's own screens: black ground, white primary
/// text, grey secondary text, thin grey rules between areas, and colour
/// only for state.
namespace palette {
inline constexpr services::Color Ground = services::rgb(0, 0, 0);
inline constexpr services::Color Text = services::rgb(255, 255, 255);
inline constexpr services::Color Dim = services::rgb(150, 150, 150);
inline constexpr services::Color Rule = services::rgb(70, 70, 70);
inline constexpr services::Color Transmit = services::rgb(210, 40, 30);
inline constexpr services::Color Receive = services::rgb(40, 170, 60);
inline constexpr services::Color Warn = services::rgb(230, 160, 0);
inline constexpr services::Color Beacon = services::rgb(40, 100, 200);
/// A panel set off from the ground, for a line of values.
inline constexpr services::Color Panel = services::rgb(28, 28, 32);
} // namespace palette

/// The GPS receiver as the screens show it.
struct GpsState
{
    bool on = false;
    /// Sentences are arriving.
    bool receiving = false;
    services::GpsFix fix;
    /// Switched off between fixes to save power.
    bool asleep = false;
};

/// What the standby screen shows besides the channel.
struct StandbyState
{
    bool transmitting = false;
    bool receiving = false;
    bool locked = false;
    /// Digits typed for a frequency, or empty.
    std::string_view entry;
    /// The quick-key hint for the bottom bar.
    std::string_view keys;
    /// A short message in the bottom bar instead of the key hints, or empty.
    std::string_view note;
    /// Volume step 0..15, or -1 when unknown.
    int volume = -1;
    /// The battery voltage, 0 while unknown, and whether it is low.
    int millivolts = 0;
    bool batteryLow = false;
    /// The volume next to the power instead of the battery voltage.
    bool volumeShown = false;
    GpsState gps;
    /// The time, "14:32" local or "11:32Z" UTC, or empty while it is
    /// unknown.
    std::string_view clock;
    /// A DMR channel's contact: "TG 244", "PC 2440001", "ALL CALL"; and the
    /// contact's name, or empty.
    std::string_view talkgroup;
    std::string_view talkgroupName;
    /// The contact's name large instead of the channel's.
    bool contactNameLarge = false;
    /// APRS beacons go out on their own.
    bool beacons = false;
    /// The channel roams: a search under way, or the repeater in use (its
    /// output frequency, 0 = the channel's own; its colour code; its name,
    /// or empty).
    bool roaming = false;
    bool roamSearching = false;
    std::uint32_t roamHz = 0;
    std::uint8_t roamColourCode = 0;
    std::string_view roamSite;
};

/// A DMR call for the call screen.
struct CallInfo
{
    /// Heard now; else the last call, shown for a moment after it ended.
    bool active = false;
    bool group = false;
    bool allCall = false;
    /// The caller's callsign or contact name, the user's name under it;
    /// empty when not known.
    std::string_view callerHeadline;
    std::string_view callerName;
    std::uint32_t callerId = 0;
    std::string_view targetName;
    std::uint32_t targetId = 0;
    /// The receive slot, 0 when both are heard.
    std::uint8_t slot = 0;
    /// How long the call has lasted, or lasted.
    std::uint32_t seconds = 0;
    std::string_view channel;
};

/// What the transmit screen shows.
struct TransmitInfo
{
    /// The channel's or the contact's name, large; on FM the channel's.
    std::string_view target;
    /// Under it: "TG 244" on DMR.
    std::string_view name;
    std::uint32_t seconds = 0;
    /// The DMR slot, 0 on FM.
    std::uint8_t slot = 0;
    const char *power = "";
};

/// The radio's main screen: a status strip, the talkgroup or channel large
/// with the time, frequency and slot, power and voltage, details, and a bar
/// naming the quick keys. Calls and transmissions take the whole screen.
class StandbyScreen
{
public:
    static constexpr int StatusH = 11;
    static constexpr int HintH = 11;

    void draw(services::Display &d, const services::ChannelSelector &sel, const StandbyState &s);
    /// Only the status strip: lock, GPS still searching, APRS beacons; the
    /// caller draws the battery at its right end.
    void drawStatus(services::Display &d, const StandbyState &s);
    /// Only the working screen's time, in place.
    void drawClock(services::Display &d, const StandbyState &s);
    /// The call screen in place of the channel: call type, caller, target,
    /// and the call's time in the bottom bar.
    void drawCall(services::Display &d, const StandbyState &s, const CallInfo &c);
    /// Only the call screen's running time.
    void drawCallTime(services::Display &d, const CallInfo &c);
    /// The transmit screen: whom, and how long.
    void drawTransmit(services::Display &d, const StandbyState &s, const TransmitInfo &t);
    /// Only the transmit screen's running time.
    void drawTransmitTime(services::Display &d, const TransmitInfo &t);
    /// Only the working screen's volume or battery voltage next to the
    /// power, in place.
    void drawReading(services::Display &d, const StandbyState &s);
    /// Where the caller may draw the battery: x of its left edge.
    int batteryX(const services::Display &d) const { return d.width() - 48; }
};

/// A list of what happened (the activity list), between the status strip
/// and a hint bar: a title with a note on its right, then LogRows rows of
/// a time and a line of text.
inline constexpr int LogRowH = 11;
inline constexpr int LogRows = 8;
void drawLogFrame(services::Display &d, std::string_view title, std::string_view note, std::string_view hint);
/// Row @p row of the list, 0 at the top; empty texts clear it. A selected
/// row stands out.
void drawLogRow(services::Display &d, int row, std::string_view time, std::string_view text, services::Color ink,
    bool selected = false);

/// The GPS screen: the receiver's state, position, altitude, satellites,
/// speed and UTC time, between the status bar and a hint bar. @p full
/// draws the labels and rules too; otherwise only the values change,
/// drawn over the old ones.
void drawGpsScreen(services::Display &d, const GpsState &gps, bool full);

/// "12.3 km" from our fix to the station, or empty without both positions.
lib::StaticString<10> distanceText(const GpsState &gps, const aprs::HeardStation &s);
/// "45 s", "12 min", "3 h": how long ago @p thenMs was.
lib::StaticString<8> ageText(std::uint32_t thenMs, std::uint32_t nowMs);
/// One heard APRS station: position, distance and bearing from our fix,
/// altitude, speed, its text and the way it came.
void drawAprsStation(services::Display &d, const aprs::HeardStation &s, const GpsState &gps, std::uint32_t nowMs);

/// A list to pick one item from, drawn over the screen body: a title band
/// with the place in the list, up to Rows rows with the chosen one
/// inverted and a scroll bar, and a hint bar.
class ListPicker
{
public:
    static constexpr int Rows = 7;
    static constexpr std::size_t MaxItems = 250;
    static constexpr std::size_t TextBytes = 17;
    static constexpr std::size_t TitleBytes = 27;

    void open(std::string_view title);
    /// Gives the open list another title, keeping its items.
    void retitle(std::string_view title);
    /// An item: @p text on the left, @p value (if any) right-aligned. Both
    /// are cut to fit their buffers: keep @p text to 16 characters and
    /// @p value to 8.
    void add(std::uint8_t id, std::string_view text, std::string_view value = {});
    void select(std::uint8_t id);
    void move(int steps);
    bool empty() const { return m_count == 0; }
    std::uint8_t chosen() const { return m_ids[m_cursor]; }
    std::size_t size() const { return m_count; }
    std::size_t cursor() const { return m_cursor; }
    std::string_view title() const { return m_title; }
    std::string_view text(std::size_t i) const { return m_text[i]; }
    std::string_view value(std::size_t i) const { return m_value[i]; }
    /// Whether any item has a value beside its text.
    bool hasValues() const;
    void draw(services::Display &d, int top, std::string_view hint);

private:
    char m_title[TitleBytes] = {};
    std::uint8_t m_ids[MaxItems] = {};
    char m_text[MaxItems][TextBytes] = {};
    static constexpr std::size_t ValueBytes = 10;
    char m_value[MaxItems][ValueBytes] = {};
    std::size_t m_count = 0;
    std::size_t m_cursor = 0;
};

/// One row of the menu: a label, its value or empty, and whether Menu
/// opens a screen of its own.
struct MenuRow
{
    std::string_view label;
    std::string_view value;
    bool opens = false;
    /// The value differs from what was read: drawn in amber.
    bool changed = false;
};
/// A screen of up to ListPicker::Rows @p rows with @p chosen inverted, under
/// a title band with @p title and the place @p index of @p count, and a
/// hint bar.
void drawListScreen(services::Display &d, std::string_view title, std::size_t index, std::size_t count,
    const MenuRow *rows, int rowCount, int chosen, std::string_view hint);
inline constexpr int MenuRows = 5;
/// The menu: a title band with @p title and the place @p index of
/// @p count, up to MenuRows @p rows with @p chosen inverted, a panel of two
/// lines about the chosen row, and a hint bar.
void drawMenuScreen(services::Display &d, std::string_view title, std::size_t index, std::size_t count,
    const MenuRow *rows, int rowCount, int chosen, std::string_view about1, std::string_view about2,
    std::string_view hint);
/// A few values as large buttons, the chosen one inverted and the one in
/// use starred.
void drawValueButtons(services::Display &d, const ListPicker &values, std::size_t inUse, std::string_view hint);
/// One value of many, large between arrows, with its place on a scale and
/// the value in use under it.
void drawValueSpinner(services::Display &d, const ListPicker &values, std::size_t inUse, std::string_view hint);
/// The same from texts: up to four @p labels as buttons.
void drawValueButtons(services::Display &d, std::string_view title, const std::string_view *labels, std::size_t count,
    std::size_t chosen, std::size_t inUse, std::string_view hint);
/// The same from texts: @p value, its place @p index of @p count, and the
/// value in use as text (empty: none shown).
void drawValueSpinner(services::Display &d, std::string_view title, std::string_view value, std::size_t index,
    std::size_t count, std::string_view inUse, bool atInUse, std::string_view hint);
/// Digits typed on the keypad over a value: @p text is the whole value as
/// shown, its first @p typed characters typed, then @p unit; a cursor under
/// the next character, and two lines of notes.
void drawDigitsEntry(services::Display &d, std::string_view title, std::string_view text, std::size_t typed,
    std::string_view unit, std::string_view note1, std::string_view note2, std::string_view hint);
/// A text typed letter by letter: @p text with a cursor on character
/// @p cursor, and the letters of the key last pressed with @p chosen of
/// them lifted.
void drawTextEntry(services::Display &d, std::string_view title, std::string_view text, std::size_t cursor,
    std::size_t maxChars, std::string_view letters, std::size_t chosen, std::string_view hint);

} // namespace dmm::app::ui
