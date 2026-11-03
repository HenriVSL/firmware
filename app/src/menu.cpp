// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Henri Lavikainen

#include "menu.h"

#include "dmm/services/gps.h"

#include <algorithm>
#include <iterator>

namespace dmm::app::ui::menu {

namespace {

using services::Key;

constexpr Row TopRows[] = {Group::Channel, Group::Roaming, Group::Gps, Group::Aprs, Group::Display, Group::Settings,
    Group::Tools};
constexpr Row ChannelRows[] = {Item::Squelch, Item::SquelchMode, Item::Power};
constexpr Row RoamingRows[] = {Item::RoamStatus, Item::Roaming, Item::RoamZone};
constexpr Row GpsRows[] = {Item::GpsStatus, Item::Gps};
constexpr Row AprsRows[] = {Item::AprsMode, Item::AprsSend, Item::AprsBeacon, Item::AprsHeard};
constexpr Row SettingsRows[] = {Group::Home, Group::Audio, Group::Radio, Group::Keys, Group::Clock, Group::GpsSettings,
    Group::AprsSettings};
constexpr Row DisplayRows[] = {Item::Backlight, Item::BacklightTime};
constexpr Row ToolsRows[] = {Group::Codeplug, Item::Activity, Item::Log, Item::KeyTest, Item::Version};
constexpr Row HomeRows[] = {Item::HomePreset, Item::HomeTop, Item::HomeClock, Item::HomeBig, Item::HomeMiddle,
    Item::HomeBeside, Item::HomeBottom};
constexpr Row AudioRows[] = {Item::DmrMicGain, Item::FmMicGain};
constexpr Row RadioRows[] = {Item::FrequencyOffset};
constexpr Row KeysRows[] = {Item::KeyLock, Item::AutoLock, Item::BatterySave};
constexpr Row ClockRows[] = {Item::TimeZone, Item::ClockShown, Item::ClockCalibration};
constexpr Row GpsSettingsRows[] = {Item::GpsSystems};
constexpr Row CodeplugRows[] = {Item::PlugChannels, Item::PlugZones, Item::PlugContacts, Item::PlugGroupLists,
    Item::PlugRepeaters, Item::PlugRoamZones, Item::PlugRadioIds, Item::PlugSettings, Item::PlugMoreSettings,
    Item::PlugAprs, Item::PlugAprsText};
constexpr Row AprsSettingsRows[] = {Item::AprsPower, Item::AprsReceive, Item::AprsAudible};

constexpr GroupInfo Groups[] = {
    {"Menu", "", TopRows},
    {"Channel", "Squelch and power of the channel in use", ChannelRows},
    {"Roaming", "Moving between the roaming zone's repeaters", RoamingRows},
    {"GPS", "The position and the GPS receiver", GpsRows},
    {"APRS", "APRS mode, sending the position, stations heard", AprsRows},
    {"Display", "Brightness and how long the light stays on", DisplayRows},
    {"Settings", "Home, audio, radio, keys, clock, GPS and APRS", SettingsRows},
    {"Tools", "Codeplug, activity, the log, key test and version", ToolsRows},
    {"Home screen", "What each row of the home screen shows", HomeRows},
    {"Audio", "The microphone gain on DMR and FM", AudioRows},
    {"Radio", "Frequency offset for a drifted reference", RadioRows},
    {"Keys & battery", "The key lock and battery save", KeysRows},
    {"Clock", "Time zone, local or UTC, and calibration", ClockRows},
    {"GPS", "The satellite systems the receiver uses", GpsSettingsRows},
    {"Codeplug", "Every record and value of the codeplug, edited", CodeplugRows},
    {"APRS", "Send power, the APRS receiver, packet sound", AprsSettingsRows},
};
static_assert(std::size(Groups) == static_cast<std::size_t>(Group::Count));

constexpr ItemInfo Items[] = {
    {"Squelch", "Open, or 1 to 5: higher needs a stronger signal", Kind::Value},
    {"Squelch type", "Carrier opens on any signal, Tone on its tone", Kind::Value},
    {"Power", "Transmit power of this channel", Kind::Value},
    {"Status", "Repeaters heard and their levels; search now", Kind::Screen},
    {"Roaming", "Roaming channels use the roaming zone's repeaters", Kind::Value},
    {"Roam zone", "The repeaters to move between", Kind::Value},
    {"Status", "Position, satellites and time from the GPS", Kind::Screen},
    {"Receiver", "The GPS receiver on or off", Kind::Value},
    {"APRS mode", "One receiver: APRS mode listens only on APRS", Kind::Value},
    {"Send now", "Sends the position once, when the channel is free", Kind::Action},
    {"Auto send", "How often the position is sent by itself", Kind::Value},
    {"Heard", "Stations heard, newest first; Menu opens one", Kind::Screen},
    {"Brightness", "Display brightness", Kind::Value},
    {"Light time", "How long the light stays on after a key", Kind::Value},
    {"DMR mic gain", "Voice level sent on DMR, from the next call", Kind::Value},
    {"FM mic gain", "Voice level sent on FM, from the next call", Kind::Value},
    {"Freq offset", "Shifts every frequency; Up and Down 500 Hz", Kind::Value},
    {"Lock covers", "What the key lock covers", Kind::Value},
    {"Auto lock", "Lock the keys after this long without use", Kind::Value},
    {"Battery save", "Sleep between checks for a signal while quiet", Kind::Value},
    {"Time zone", "Local time's offset from UTC", Kind::Value},
    {"Shown as", "Home screen time: local, or UTC with a Z", Kind::Value},
    {"Calibrate", "Times the clock against GPS and corrects its rate", Kind::Value},
    {"Satellites", "Satellite systems the receiver uses", Kind::Value},
    {"Send power", "Transmit power for sending the position", Kind::Value},
    {"Receive", "The APRS receiver lists the stations it hears", Kind::Value},
    {"Hear packets", "In APRS mode: packets heard on the speaker", Kind::Value},
    {"Preset", "A whole layout at once; rows can then change", Kind::Value},
    {"Top line", "Small line at the top: talkgroup, zone, place", Kind::Value},
    {"Clock", "The time at the top right", Kind::Value},
    {"Large name", "The large name: channel, contact or zone", Kind::Value},
    {"Middle row", "Frequency, contact or signal under the name", Kind::Value},
    {"Next to power", "Voltage, volume or signal next to the power", Kind::Value},
    {"Bottom row", "Details, last caller or GPS position", Kind::Value},
    {"Channels", "Every channel: names, frequencies, DMR, tones", Kind::Screen},
    {"Zones", "Zone names", Kind::Screen},
    {"Contacts", "Contact names and numbers", Kind::Screen},
    {"RX groups", "Receive group lists", Kind::Screen},
    {"Repeaters", "The roaming zones' repeaters", Kind::Screen},
    {"Roam zones", "Roaming zone names", Kind::Screen},
    {"Radio IDs", "The radio's own DMR IDs", Kind::Screen},
    {"Settings", "The radio's settings; saving restarts it", Kind::Screen},
    {"More settings", "Further settings: GPS; saving restarts it", Kind::Screen},
    {"APRS", "APRS station and beacon", Kind::Screen},
    {"APRS text", "The text sent with the position", Kind::Screen},
    {"Activity", "Calls, roaming moves and GPS fixes, newest first", Kind::Screen},
    {"Log", "The radio's log; hold Exit for the next screen", Kind::Screen},
    {"Key test", "Shows each key pressed; hold Exit for the next", Kind::Screen},
    {"Version", "The firmware's version and time since start", Kind::Info},
};
static_assert(std::size(Items) == static_cast<std::size_t>(Item::Count));

// Choices with fixed texts.
struct Fixed
{
    std::int32_t value;
    std::string_view text;
    std::string_view note = {};
};
constexpr Fixed OffOn[] = {{0, "Off"}, {1, "On"}};
constexpr Fixed SquelchModes[] = {{static_cast<std::int32_t>(services::SquelchMode::Carrier), "Carrier"},
    {static_cast<std::int32_t>(services::SquelchMode::Tone), "Tone"}};
constexpr Fixed Powers[] = {{0, "Low"}, {1, "Mid"}, {2, "High"}, {3, "Turbo"}};
constexpr Fixed LockCovers[] = {{0, "All"}, {1, "Keys+PTT"}, {2, "Keypad"}, {3, "Side+PTT"}};
constexpr Fixed AutoLocks[] = {{0, "Off"}, {10, "10 s"}, {30, "30 s"}, {60, "60 s"}};
constexpr Fixed ClockShowns[] = {{0, "Local"}, {1, "UTC"}};
// Calibration runs in minutes; 0 stops the one under way.
constexpr Fixed Calibrations[] = {{30, "30 min"}, {60, "60 min"}, {0, "Stop"}};
constexpr Fixed BatterySaves[] = {{0, "Off"}, {1, "On", "150 ms"}, {2, "Fast wake", "70 ms"}};
namespace g = services::gps_systems;
constexpr Fixed GpsSystems[] = {{g::Gps | g::Beidou, "GPS + BeiDou"}, {g::Gps, "GPS"}, {g::Beidou, "BeiDou"},
    {g::Gps | g::Glonass, "GPS + GLONASS"}, {g::Glonass, "GLONASS"}, {g::Beidou | g::Glonass, "BeiDou + GLONASS"},
    {g::All, "All three"}};
// The home screen's choices, numbered as services::HomeLayout's.
constexpr Fixed HomePresets[] = {{static_cast<std::int32_t>(services::HomePreset::Detailed), "Detailed"},
    {static_cast<std::int32_t>(services::HomePreset::Simple), "Simple"},
    {static_cast<std::int32_t>(services::HomePreset::Roaming), "Roaming"}};
constexpr Fixed HomeTops[] = {{0, "Talkgroup"}, {1, "Zone"}, {2, "Number"}, {3, "None"}};
constexpr Fixed HomeBigs[] = {{0, "Channel"}, {1, "Contact"}, {2, "Zone"}};
constexpr Fixed HomeMiddles[] = {{0, "Frequency"}, {1, "Talkgroup"}, {2, "Signal"}};
constexpr Fixed HomeBesides[] = {{0, "Voltage"}, {1, "Volume"}, {2, "Signal"}};
constexpr Fixed HomeBottoms[] = {{0, "Details"}, {1, "Last caller"}, {2, "Position"}, {3, "None"}};
static_assert(std::size(HomeTops) == static_cast<std::size_t>(services::HomeLayout::Top::Count));
static_assert(std::size(HomeBigs) == static_cast<std::size_t>(services::HomeLayout::Big::Count));
static_assert(std::size(HomeMiddles) == static_cast<std::size_t>(services::HomeLayout::Middle::Count));
static_assert(std::size(HomeBesides) == static_cast<std::size_t>(services::HomeLayout::Beside::Count));
static_assert(std::size(HomeBottoms) == static_cast<std::size_t>(services::HomeLayout::Bottom::Count));
// Light time choices, in seconds.
constexpr std::uint16_t BacklightTimes[] = {0, 5, 10, 15, 20, 25, 30, 60, 120, 180, 240, 300, 900, 1800, 2700, 3600};
// Automatic APRS beacon intervals, in seconds; 0 = off.
constexpr std::uint16_t BeaconIntervals[] = {0, 60, 120, 300, 600, 900, 1200, 1800, 3600};
// Time zones: UTC-12:00 to UTC+14:00 in half hours.
constexpr int ZoneFirstMinutes = -12 * 60;
constexpr int ZoneLastMinutes = 14 * 60;
constexpr int ZoneStepMinutes = 30;
constexpr int MaxLevel = 5;
namespace mic = services::microphone_gain;
// What Up and Down move the frequency offset: the whole steps nearest 500 Hz.
constexpr std::uint32_t OffsetCoarseMilliHz = 500'000;
constexpr std::size_t OffsetCoarseSteps =
    std::max<std::size_t>(1, (OffsetCoarseMilliHz + OffsetSteps.stepMilliHz() / 2) / OffsetSteps.stepMilliHz());

std::span<const Fixed> fixedChoices(Item item)
{
    switch (item) {
    case Item::SquelchMode:
        return SquelchModes;
    case Item::Power:
        return Powers;
    case Item::Roaming:
    case Item::Gps:
    case Item::HomeClock:
    case Item::AprsReceive:
    case Item::AprsMode:
    case Item::AprsAudible:
        return OffOn;
    case Item::AprsPower:
        return Powers;
    case Item::HomePreset:
        return HomePresets;
    case Item::HomeTop:
        return HomeTops;
    case Item::HomeBig:
        return HomeBigs;
    case Item::HomeMiddle:
        return HomeMiddles;
    case Item::HomeBeside:
        return HomeBesides;
    case Item::HomeBottom:
        return HomeBottoms;
    case Item::KeyLock:
        return LockCovers;
    case Item::AutoLock:
        return AutoLocks;
    case Item::BatterySave:
        return BatterySaves;
    case Item::GpsSystems:
        return GpsSystems;
    case Item::ClockShown:
        return ClockShowns;
    case Item::ClockCalibration:
        return Calibrations;
    default:
        return {};
    }
}

// The offset applied for a kept @p hz.
ValueText offsetText(std::int32_t hz)
{
    services::FrequencyOffset offset = OffsetSteps;
    offset.setHz(hz);
    return offset.text();
}

// The offset choices, from the most negative step up.
services::FrequencyOffset offsetChoice(std::size_t i)
{
    services::FrequencyOffset offset = OffsetSteps;
    offset.setSteps(static_cast<int>(i) - offset.maxSteps());
    return offset;
}
std::int32_t offsetChoiceValue(std::size_t i)
{
    return offsetChoice(i).hz();
}
ValueText offsetChoiceText(std::size_t i)
{
    return offsetChoice(i).text();
}

// "Always", "5 s", "2 min", "1 h".
ValueText durationText(std::int32_t seconds)
{
    ValueText t;
    if (seconds == 0) {
        t << "Always";
    } else if (seconds >= 3600) {
        t << seconds / 3600 << " h";
    } else if (seconds >= 60) {
        t << seconds / 60 << " min";
    } else {
        t << seconds << " s";
    }
    return t;
}

// "UTC+3:00", "UTC-9:30", "UTC".
ValueText zoneText(std::int32_t minutes)
{
    ValueText t;
    t << "UTC";
    if (minutes != 0) {
        const std::int32_t m = minutes < 0 ? -minutes : minutes;
        t << (minutes < 0 ? '-' : '+') << m / 60 << ':' << lib::zeroPadded(static_cast<std::uint32_t>(m % 60), 2);
    }
    return t;
}

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

const GroupInfo &info(Group group)
{
    return Groups[static_cast<std::size_t>(group)];
}

const ItemInfo &info(Item item)
{
    return Items[static_cast<std::size_t>(item)];
}

std::string_view label(Row row)
{
    return row.opensGroup() ? info(row.group).name : info(row.item).label;
}

std::string_view about(Row row)
{
    return row.opensGroup() ? info(row.group).about : info(row.item).about;
}

Place parentOf(Group group)
{
    for (std::size_t g = 0; g < std::size(Groups); ++g) {
        const std::span<const Row> rows = Groups[g].rows;
        for (std::size_t r = 0; r < rows.size(); ++r) {
            if (rows[r].group == group) {
                return {static_cast<Group>(g), r};
            }
        }
    }
    return {Group::Count, 0};
}

Title groupTitle(Group group)
{
    Title t;
    const Place parent = parentOf(group);
    const std::string_view name = info(group).name;
    if (parent.group != Group::Count) {
        const std::string_view above = info(parent.group).name;
        if (above.size() + 3 + name.size() <= TitleChars) {
            t << above << " > ";
        }
    }
    t << name;
    return t;
}

Title valueTitle(Group group, Item item)
{
    Title t;
    const std::string_view name = info(group).name;
    const std::string_view text = info(item).label;
    if (name.size() + 3 + text.size() <= TitleChars) {
        t << name << " > ";
    }
    t << text;
    return t;
}

bool splitAbout(std::string_view text, std::string_view &first, std::string_view &second)
{
    if (text.size() <= AboutChars) {
        first = text;
        second = {};
        return true;
    }
    // The last space that leaves the first line short enough.
    std::size_t at = AboutChars;
    while (at > 0 && text[at] != ' ') {
        --at;
    }
    if (at == 0) {
        return false;
    }
    first = std::string_view(text.data(), at);
    second = std::string_view(text.data() + at + 1, text.size() - at - 1);
    return second.size() <= AboutChars;
}

services::plug::Book bookOf(Item item)
{
    constexpr auto First = static_cast<std::size_t>(Item::PlugChannels);
    static_assert(static_cast<std::size_t>(Item::PlugAprsText) - First + 1 ==
        static_cast<std::size_t>(services::plug::Book::Count));
    const auto i = static_cast<std::size_t>(item);
    return i >= First && i <= static_cast<std::size_t>(Item::PlugAprsText) ? static_cast<services::plug::Book>(i - First)
                                                                           : services::plug::Book::Count;
}

Item itemOf(services::plug::Book book)
{
    return static_cast<Item>(static_cast<std::size_t>(Item::PlugChannels) + static_cast<std::size_t>(book));
}

Kept keptIn(Item item)
{
    switch (item) {
    case Item::SquelchMode:
    case Item::Power:
        return Kept::Channel;
    case Item::Roaming:
    case Item::HomePreset:
    case Item::HomeTop:
    case Item::HomeClock:
    case Item::HomeBig:
    case Item::HomeMiddle:
    case Item::HomeBeside:
    case Item::HomeBottom:
    case Item::DmrMicGain:
    case Item::FmMicGain:
    case Item::FrequencyOffset:
    case Item::ClockShown:
    case Item::AprsReceive:
    case Item::AprsMode:
    case Item::AprsAudible:
        return Kept::Preferences;
    case Item::AprsBeacon:
    case Item::AprsPower:
        return Kept::Aprs;
    case Item::Squelch:
    case Item::RoamZone:
    case Item::Gps:
    case Item::Backlight:
    case Item::BacklightTime:
    case Item::KeyLock:
    case Item::AutoLock:
    case Item::BatterySave:
    case Item::TimeZone:
    case Item::GpsSystems:
        return Kept::Settings;
    case Item::ClockCalibration:
    case Item::RoamStatus:
    case Item::GpsStatus:
    case Item::PlugChannels:
    case Item::PlugZones:
    case Item::PlugContacts:
    case Item::PlugGroupLists:
    case Item::PlugRepeaters:
    case Item::PlugRoamZones:
    case Item::PlugRadioIds:
    case Item::PlugSettings:
    case Item::PlugMoreSettings:
    case Item::PlugAprs:
    case Item::PlugAprsText:
    case Item::AprsSend:
    case Item::AprsHeard:
    case Item::Activity:
    case Item::Log:
    case Item::KeyTest:
    case Item::Version:
    case Item::Count:
        break;
    }
    return Kept::No;
}

std::int32_t choiceOf(Item item, const Settings &s)
{
    const services::KeyLockSettings &k = s.keyLock;
    switch (item) {
    case Item::Squelch:
        return s.squelch;
    case Item::SquelchMode:
        return static_cast<std::int32_t>(s.squelchMode);
    case Item::Power:
        return static_cast<std::int32_t>(s.power);
    case Item::Roaming:
        return s.roaming ? 1 : 0;
    case Item::RoamZone:
        return s.roamZone;
    case Item::Gps:
        return s.gps ? 1 : 0;
    case Item::Backlight:
        return s.backlight;
    case Item::BacklightTime:
        return s.backlightS;
    case Item::DmrMicGain:
        return s.dmrMicGainDb ? *s.dmrMicGainDb : FromCodeplug + s.plugDmrMicGainDb;
    case Item::FmMicGain:
        return s.fmMicGainDb ? *s.fmMicGainDb : FromCodeplug + s.plugFmMicGainDb;
    case Item::KeyLock:
        return k.knob ? 0 : k.keypad && k.side ? 1
            : k.keypad                         ? 2
                                               : 3;
    case Item::AutoLock:
        return static_cast<std::int32_t>(k.autoLockMs / 1000);
    case Item::BatterySave:
        return s.batterySave;
    case Item::TimeZone:
        return s.timeZoneMinutes;
    case Item::ClockShown:
        return s.clockUtc ? 1 : 0;
    case Item::ClockCalibration:
        return s.clockCalibrationMinutes;
    case Item::GpsSystems:
        return s.gpsSystems;
    case Item::HomePreset:
        return static_cast<std::int32_t>(services::presetOf(s.home));
    case Item::HomeTop:
        return static_cast<std::int32_t>(s.home.top);
    case Item::HomeClock:
        return s.home.clock ? 1 : 0;
    case Item::HomeBig:
        return static_cast<std::int32_t>(s.home.big);
    case Item::HomeMiddle:
        return static_cast<std::int32_t>(s.home.middle);
    case Item::HomeBeside:
        return static_cast<std::int32_t>(s.home.beside);
    case Item::HomeBottom:
        return static_cast<std::int32_t>(s.home.bottom);
    case Item::FrequencyOffset:
        return s.frequencyOffsetHz;
    case Item::AprsBeacon:
        return s.beaconIntervalS;
    case Item::AprsPower:
        return static_cast<std::int32_t>(s.beaconPower);
    case Item::AprsReceive:
        return s.aprsReceive ? 1 : 0;
    case Item::AprsMode:
        return s.aprsMode ? 1 : 0;
    case Item::AprsAudible:
        return s.aprsAudible ? 1 : 0;
    case Item::RoamStatus:
    case Item::GpsStatus:
    case Item::PlugChannels:
    case Item::PlugZones:
    case Item::PlugContacts:
    case Item::PlugGroupLists:
    case Item::PlugRepeaters:
    case Item::PlugRoamZones:
    case Item::PlugRadioIds:
    case Item::PlugSettings:
    case Item::PlugMoreSettings:
    case Item::PlugAprs:
    case Item::PlugAprsText:
    case Item::AprsSend:
    case Item::AprsHeard:
    case Item::Activity:
    case Item::Log:
    case Item::KeyTest:
    case Item::Version:
    case Item::Count:
        break;
    }
    return 0;
}

void setChoice(Item item, std::int32_t value, Settings &s)
{
    const auto byte = static_cast<std::uint8_t>(value);
    switch (item) {
    case Item::Squelch:
        s.squelch = byte;
        break;
    case Item::SquelchMode:
        s.squelchMode = static_cast<services::SquelchMode>(byte);
        break;
    case Item::Power:
        s.power = static_cast<services::PowerLevel>(byte);
        break;
    case Item::Roaming:
        s.roaming = value != 0;
        break;
    case Item::RoamZone:
        s.roamZone = byte;
        break;
    case Item::Gps:
        s.gps = value != 0;
        break;
    case Item::Backlight:
        s.backlight = byte;
        break;
    case Item::BacklightTime:
        s.backlightS = static_cast<std::uint16_t>(value);
        break;
    case Item::DmrMicGain:
        s.dmrMicGainDb = value < FromCodeplug ? std::optional(byte) : std::nullopt;
        break;
    case Item::FmMicGain:
        s.fmMicGainDb = value < FromCodeplug ? std::optional(byte) : std::nullopt;
        break;
    case Item::KeyLock:
        s.keyLock.keypad = value != 3;
        s.keyLock.side = value != 2;
        s.keyLock.knob = value == 0;
        break;
    case Item::AutoLock:
        s.keyLock.autoLockMs = static_cast<std::uint32_t>(value) * 1000;
        break;
    case Item::BatterySave:
        s.batterySave = byte;
        break;
    case Item::TimeZone:
        s.timeZoneMinutes = static_cast<std::int16_t>(value);
        break;
    case Item::ClockShown:
        s.clockUtc = value != 0;
        break;
    case Item::ClockCalibration:
        s.clockCalibrationMinutes = byte;
        break;
    case Item::GpsSystems:
        s.gpsSystems = byte;
        break;
    case Item::HomePreset:
        s.home = services::presetLayout(static_cast<services::HomePreset>(byte));
        break;
    case Item::HomeTop:
        s.home.top = static_cast<services::HomeLayout::Top>(byte);
        break;
    case Item::HomeClock:
        s.home.clock = value != 0;
        break;
    case Item::HomeBig:
        s.home.big = static_cast<services::HomeLayout::Big>(byte);
        break;
    case Item::HomeMiddle:
        s.home.middle = static_cast<services::HomeLayout::Middle>(byte);
        break;
    case Item::HomeBeside:
        s.home.beside = static_cast<services::HomeLayout::Beside>(byte);
        break;
    case Item::HomeBottom:
        s.home.bottom = static_cast<services::HomeLayout::Bottom>(byte);
        break;
    case Item::FrequencyOffset:
        s.frequencyOffsetHz = static_cast<std::int16_t>(value);
        break;
    case Item::AprsBeacon:
        s.beaconIntervalS = static_cast<std::uint16_t>(value);
        break;
    case Item::AprsPower:
        s.beaconPower = static_cast<services::PowerLevel>(byte & 3);
        break;
    case Item::AprsReceive:
        s.aprsReceive = value != 0;
        break;
    case Item::AprsMode:
        s.aprsMode = value != 0;
        break;
    case Item::AprsAudible:
        s.aprsAudible = value != 0;
        break;
    case Item::RoamStatus:
    case Item::GpsStatus:
    case Item::PlugChannels:
    case Item::PlugZones:
    case Item::PlugContacts:
    case Item::PlugGroupLists:
    case Item::PlugRepeaters:
    case Item::PlugRoamZones:
    case Item::PlugRadioIds:
    case Item::PlugSettings:
    case Item::PlugMoreSettings:
    case Item::PlugAprs:
    case Item::PlugAprsText:
    case Item::AprsSend:
    case Item::AprsHeard:
    case Item::Activity:
    case Item::Log:
    case Item::KeyTest:
    case Item::Version:
    case Item::Count:
        break;
    }
}

ValueText valueText(Item item, std::int32_t value)
{
    ValueText t;
    switch (item) {
    case Item::Squelch:
        if (value == 0) {
            t << "Open";
        } else {
            t << value;
        }
        return t;
    case Item::Backlight:
        t << value;
        return t;
    case Item::BacklightTime:
        return durationText(value);
    case Item::AprsBeacon:
        if (value == 0) {
            t << "Off";
            return t;
        }
        return durationText(value);
    case Item::DmrMicGain:
    case Item::FmMicGain:
        if (value >= FromCodeplug) {
            t << "Plug ";
            value -= FromCodeplug;
        }
        t << '+' << value << " dB";
        return t;
    case Item::AutoLock:
        if (value == 0) {
            t << "Off";
        } else {
            t << value << " s";
        }
        return t;
    case Item::TimeZone:
        return zoneText(value);
    case Item::GpsSystems:
        t << services::gps_systems::name(static_cast<std::uint8_t>(value));
        return t;
    case Item::HomePreset:
        if (value == static_cast<std::int32_t>(services::HomePreset::Own)) {
            t << "Own";
            return t;
        }
        break;
    case Item::FrequencyOffset:
        return offsetText(value);
    default:
        break;
    }
    for (const Fixed &f : fixedChoices(item)) {
        if (f.value == value) {
            t << f.text;
        }
    }
    return t;
}

void Choices::add(std::int32_t value, std::string_view text, std::string_view note)
{
    if (m_count >= MaxChoices || m_madeValue != nullptr) {
        return;
    }
    m_value[m_count] = value;
    copyText(m_text[m_count], text);
    copyText(m_note[m_count], note);
    ++m_count;
}

void Choices::make(std::size_t count, std::int32_t (*valueOf)(std::size_t), ValueText (*textOf)(std::size_t))
{
    m_count = count;
    m_madeValue = valueOf;
    m_madeText = textOf;
}

bool Choices::hasNotes() const
{
    if (m_madeValue != nullptr) {
        return false;
    }
    for (std::size_t i = 0; i < m_count; ++i) {
        if (m_note[i][0] != '\0') {
            return true;
        }
    }
    return false;
}

std::size_t Choices::find(std::int32_t value) const
{
    for (std::size_t i = 0; i < m_count; ++i) {
        if (this->value(i) == value) {
            return i;
        }
    }
    return m_count;
}

bool listChoices(Item item, Choices &out, const Settings &s)
{
    out.clear();
    switch (item) {
    case Item::RoamZone:
        return false;
    case Item::Squelch:
        for (std::int32_t level = 0; level <= MaxLevel; ++level) {
            out.add(level, valueText(item, level).view());
        }
        return true;
    case Item::Backlight:
        for (std::int32_t level = 1; level <= MaxLevel; ++level) {
            out.add(level, valueText(item, level).view());
        }
        return true;
    case Item::BacklightTime:
        for (const std::uint16_t seconds : BacklightTimes) {
            out.add(seconds, durationText(seconds).view());
        }
        return true;
    case Item::AprsBeacon:
        for (const std::uint16_t seconds : BeaconIntervals) {
            out.add(seconds, valueText(item, seconds).view());
        }
        return true;
    case Item::TimeZone:
        for (int m = ZoneFirstMinutes; m <= ZoneLastMinutes; m += ZoneStepMinutes) {
            out.add(m, zoneText(m).view());
        }
        return true;
    case Item::DmrMicGain:
    case Item::FmMicGain: {
        const bool dmr = item == Item::DmrMicGain;
        const std::int32_t plug = FromCodeplug + (dmr ? s.plugDmrMicGainDb : s.plugFmMicGainDb);
        ValueText text;
        text << "Codeplug " << valueText(item, plug - FromCodeplug).view();
        out.add(plug, text.view());
        for (std::int32_t db = mic::MinDb; db <= (dmr ? mic::MaxDigitalDb : mic::MaxFmDb); ++db) {
            out.add(db, valueText(item, db).view());
        }
        return true;
    }
    case Item::ClockCalibration:
        // Stop only while a run is under way.
        for (const Fixed &f : Calibrations) {
            if (f.value != 0 || s.clockCalibrationMinutes != 0) {
                out.add(f.value, f.text);
            }
        }
        return true;
    case Item::FrequencyOffset:
        out.make(2 * static_cast<std::size_t>(OffsetSteps.maxSteps()) + 1, offsetChoiceValue, offsetChoiceText);
        out.setScale(OffsetCoarseSteps);
        return true;
    default:
        break;
    }
    for (const Fixed &f : fixedChoices(item)) {
        out.add(f.value, f.text, f.note);
    }
    return true;
}

Style styleOf(const Choices &choices)
{
    if (choices.hasNotes()) {
        return Style::List;
    }
    return choices.size() <= 4 ? Style::Buttons : Style::Spinner;
}

void Navigator::open(Group group, bool quick)
{
    m_open = true;
    m_quick = quick;
    m_choosing = false;
    m_group = group;
}

void Navigator::choose(std::int32_t inUse)
{
    const std::size_t at = m_choices.find(inUse);
    m_inUse = at;
    m_cursor = at < m_choices.size() ? at : 0;
    m_choosing = m_choices.size() > 0;
}

Navigator::Step Navigator::press(Key key)
{
    if (!m_open) {
        return Step::None;
    }
    switch (key) {
    case Key::Up:
    case Key::Down: {
        // A large value goes up with Up; lists and buttons move as drawn.
        const int up = m_choosing && style() == Style::Spinner ? 1 : -1;
        const int by = m_choosing && m_choices.coarse() > 0 ? static_cast<int>(m_choices.coarse()) : 1;
        return move(key == Key::Up ? up * by : -up * by);
    }
    case Key::Menu:
        return select();
    case Key::Exit:
        return back();
    default:
        return Step::None;
    }
}

Navigator::Step Navigator::turn(int steps)
{
    return m_open ? move(steps) : Step::None;
}

Navigator::Step Navigator::move(int steps)
{
    const int n = static_cast<int>(m_choosing ? m_choices.size() : groupInfo().rows.size());
    if (n == 0 || steps == 0) {
        return Step::None;
    }
    const int from = static_cast<int>(m_choosing ? m_cursor : row());
    const bool scale = m_choosing && m_choices.coarse() > 0;
    const int to = scale ? std::clamp(from + steps, 0, n - 1) : ((from + steps) % n + n) % n;
    if (scale && to == from) {
        return Step::None;
    }
    if (m_choosing) {
        m_cursor = static_cast<std::size_t>(to);
    } else {
        m_rows[static_cast<std::size_t>(m_group)] = static_cast<std::uint8_t>(to);
    }
    return Step::Moved;
}

Navigator::Step Navigator::select()
{
    if (m_choosing) {
        m_choosing = false;
        if (m_quick) {
            m_open = false;
        }
        return Step::Set;
    }
    const Row r = current();
    if (r.opensGroup()) {
        m_group = r.group;
        return Step::Opened;
    }
    switch (info(r.item).kind) {
    case Kind::Value:
        return Step::Choose;
    case Kind::Screen:
    case Kind::Action:
        return Step::Show;
    case Kind::Info:
        break;
    }
    return Step::None;
}

Navigator::Step Navigator::back()
{
    if (m_choosing) {
        m_choosing = false;
        return Step::Opened;
    }
    const Place parent = parentOf(m_group);
    if (m_quick || parent.group == Group::Count) {
        m_open = false;
        return Step::Left;
    }
    m_group = parent.group;
    m_rows[static_cast<std::size_t>(m_group)] = static_cast<std::uint8_t>(parent.row);
    return Step::Opened;
}

std::string_view Navigator::hint() const
{
    if (m_choosing) {
        return "Menu: set   Exit: back";
    }
    const Row r = current();
    if (r.opensGroup() || info(r.item).kind == Kind::Screen) {
        return "Menu: open   Exit: back";
    }
    if (info(r.item).kind == Kind::Value) {
        return "Menu: change   Exit: back";
    }
    if (info(r.item).kind == Kind::Action) {
        return "Menu: send   Exit: back";
    }
    return "Exit: back";
}

} // namespace dmm::app::ui::menu
