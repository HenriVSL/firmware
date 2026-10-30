// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Henri Lavikainen

#include "dmm/services/d878uv2_plug_editor.h"

#include "d878uv2_layout.h"

#include "dmm/lib/format.h"
#include "dmm/services/log.h"

#include <algorithm>
#include <cstring>
#include <iterator>

namespace dmm::services {

using namespace d878uv2;
using plug::Book;
using plug::FieldKind;

namespace {

// How a field's value sits in the record.
enum class Codec : std::uint8_t
{
    Bits,        // an unsigned bit field
    NotBit,      // a bit that is set for off
    Frequency,   // eight BCD digits of 10 Hz, big-endian
    RxFrequency, // a channel's receive frequency; its transmit one stays
    TxFrequency, // a channel's transmit frequency, as offset and direction
    Text,        // zero-terminated
    PaddedText,  // space padded
    Index32,     // a little-endian 32-bit record index
    IndexByte,   // a record index in one byte; 0xFF for none
    RxTone,      // a channel's tones: kind bits, CTCSS index, DCS code
    TxTone,
    BcdNumber, // eight BCD digits, big-endian
    TimeZone,  // half hours from UTC-12:00
    KeyCode,   // a programmable key's function code
};

} // namespace

struct D878Uv2PlugEditor::Field
{
    const char *label;
    const char *page;
    FieldKind kind;
    Codec codec;
    std::uint16_t at;
    std::uint8_t shift = 0;
    std::uint8_t bits = 8; // a bit field's width, a text's length
    std::int16_t min = 0;
    std::int16_t max = 0;
    // A number is shown as (value + bias) * scale and the unit.
    std::int16_t scale = 1;
    std::int16_t bias = 0;
    const char *unit = "";
    const char *const *names = nullptr;
    Book link = Book::Count;
    bool readOnly = false;
    bool offAtZero = false;
};

namespace {

using Field = D878Uv2PlugEditor::Field;

constexpr const char *OffOn[] = {"Off", "On"};
constexpr const char *Modes[] = {"FM", "DMR", "FM+DMR", "DMR+FM"};
constexpr const char *Powers[] = {"Low", "Mid", "High", "Turbo"};
constexpr const char *Bandwidths[] = {"12.5 kHz", "25 kHz"};
constexpr const char *Slots[] = {"TS1", "TS2"};
constexpr const char *SquelchModes[] = {"Carrier", "Tone", "Signalling", "Tone and sig.", "Tone or sig."};
constexpr const char *CallTypes[] = {"Private", "Group", "All call"};
constexpr const char *Admits[] = {"Always", "Channel free", "Other CC", "Same CC"};
constexpr const char *FmAdmits[] = {"Always", "Tone", "Channel free"};
constexpr const char *PttIds[] = {"Off", "Start", "End", "Both"};
constexpr const char *AprsKinds[] = {"Off", "FM", "DMR"};
constexpr const char *AprsPtts[] = {"Off", "Start", "End"};
constexpr const char *AlertTypes[] = {"None", "Ring", "Online"};
constexpr const char *AutoOffs[] = {"Off", "10 min", "30 min", "1 h", "2 h"};
constexpr const char *VfoSteps[] = {"2.5 kHz", "5 kHz", "6.25 kHz", "8.33 kHz", "10 kHz", "12.5 kHz", "20 kHz",
    "25 kHz", "50 kHz"};
constexpr const char *Tbsts[] = {"1000 Hz", "1450 Hz", "1750 Hz", "2100 Hz"};
constexpr const char *Languages[] = {"English", "German"};
constexpr const char *Shows[] = {"Name", "Frequency"};
constexpr const char *Callers[] = {"Off", "ID", "Call", "Both"};
constexpr const char *NameColours[] = {"Orange", "Red", "Yellow", "Green", "Turquoise", "Blue", "White"};
constexpr const char *TextColours[] = {"White", "Black", "Orange", "Red", "Yellow", "Green", "Turquoise", "Blue"};
constexpr const char *Units[] = {"Metric", "Imperial"};
constexpr const char *TalkPermits[] = {"Off", "DMR", "FM", "Both"};
constexpr const char *SlotMatches[] = {"Off", "Single", "Both"};
constexpr const char *SmsFormats[] = {"Motorola", "Hytera", "DMR"};
constexpr const char *BootDisplays[] = {"Default", "Text", "Image"};
constexpr const char *AliasShows[] = {"Off", "User DB", "Air"};
constexpr const char *AliasCodings[] = {"ISO-8", "ISO-7", "Unicode"};
constexpr const char *DateFormats[] = {"Year first", "Day first"};
constexpr const char *PositionSources[] = {"GPS", "Fixed"};
constexpr const char *NorthSouth[] = {"North", "South"};
constexpr const char *EastWest[] = {"East", "West"};
// The programmable keys' functions by code.
constexpr const char *KeyFunctions[] = {"Off", "Battery", "Power", "Talkaround", "Reverse", "Encryption", "Call",
    "VOX", "VFO", "Sub PTT", "Scan", "FM radio", "Alarm", "Record on/off", "Record", "SMS", "Dial", "GPS info",
    "Monitor", "Main channel", "Hot key 1", "Hot key 2", "Hot key 3", "Hot key 4", "Hot key 5", "Hot key 6",
    "Work alone", "Skip channel", "DMR monitor", "Sub channel", "Priority zone", "VFO scan", "Mic quality",
    "Last call", "Channel type", "Ranging", "Roam", "Channel range", "Max volume", "Slot", "APRS type", "Zone",
    "Roam set", "APRS set", "Mute", "Tone set", "TBST", "Bluetooth", "GPS", "Channel name", "CDT scan",
    "APRS send", "APRS info", "GPS roaming", "Dim", "Code 37", "Squelch"};
constexpr const char *BatterySaves[] = {"Off", "On", "Fast wake"};
constexpr const char *LightTimes[] = {"Always", "5 s", "10 s", "15 s", "20 s", "25 s", "30 s", "1 min", "2 min",
    "3 min", "4 min", "5 min", "15 min", "30 min", "45 min", "1 h"};
constexpr const char *Sides[] = {"A", "B"};
constexpr const char *VfoModes[] = {"Memory", "VFO"};
constexpr const char *RoamStarts[] = {"Periodic", "Out of range"};
constexpr const char *GpsSystems[] = {"GPS", "BeiDou", "GPS+BeiDou", "GLONASS", "GPS+GLONASS", "BeiDou+GLONASS",
    "All three"};

constexpr Field choice(const char *label, const char *page, std::uint16_t at, std::uint8_t shift, std::uint8_t bits,
    const char *const *names, std::size_t count)
{
    Field f{label, page, FieldKind::Choice, Codec::Bits, at, shift, bits};
    f.max = static_cast<std::int16_t>(count - 1);
    f.names = names;
    return f;
}
template <std::size_t N>
constexpr Field choice(const char *label, const char *page, std::uint16_t at, std::uint8_t shift, std::uint8_t bits,
    const char *const (&names)[N])
{
    return choice(label, page, at, shift, bits, names, N);
}
constexpr Field onOff(const char *label, const char *page, std::uint16_t at, std::uint8_t shift = 0,
    bool inverted = false)
{
    Field f = choice(label, page, at, shift, 1, OffOn);
    f.codec = inverted ? Codec::NotBit : Codec::Bits;
    return f;
}
constexpr Field number(const char *label, const char *page, std::uint16_t at, std::int16_t min, std::int16_t max,
    std::int16_t scale = 1, std::int16_t bias = 0, const char *unit = "", bool offAtZero = false)
{
    Field f{label, page, FieldKind::Number, Codec::Bits, at};
    f.min = min;
    f.max = max;
    f.scale = scale;
    f.bias = bias;
    f.unit = unit;
    f.offAtZero = offAtZero;
    return f;
}
constexpr Field bits(const char *label, const char *page, std::uint16_t at, std::uint8_t shift, std::uint8_t width,
    std::int16_t max)
{
    Field f = number(label, page, at, 0, max);
    f.shift = shift;
    f.bits = width;
    return f;
}
constexpr Field text(const char *label, const char *page, std::uint16_t at, std::uint8_t length, bool padded = false)
{
    Field f{label, page, FieldKind::Text, padded ? Codec::PaddedText : Codec::Text, at};
    f.bits = length;
    return f;
}
constexpr Field frequency(const char *label, const char *page, std::uint16_t at, Codec codec = Codec::Frequency)
{
    return Field{label, page, FieldKind::Frequency, codec, at};
}
constexpr Field link(const char *label, const char *page, std::uint16_t at, Book book, Codec codec = Codec::IndexByte)
{
    Field f{label, page, FieldKind::Link, codec, at};
    f.min = -1;
    f.link = book;
    return f;
}
constexpr Field special(const char *label, const char *page, FieldKind kind, Codec codec, std::uint16_t at,
    std::int16_t max)
{
    Field f{label, page, kind, codec, at};
    f.max = max;
    return f;
}
constexpr Field readOnly(Field f)
{
    f.readOnly = true;
    return f;
}

// CTCSS tones by their codeplug index, in 0.1 Hz; index 51 is the
// channel's own tone.
constexpr std::uint16_t Ctcss[] = {625, 670, 693, 719, 744, 770, 797, 825, 854, 885, 915, 948, 974, 1000, 1035,
    1072, 1109, 1148, 1188, 1230, 1273, 1318, 1365, 1413, 1462, 1514, 1567, 1598, 1622, 1655, 1679, 1713, 1738,
    1773, 1799, 1835, 1862, 1899, 1928, 1966, 1995, 2035, 2065, 2107, 2181, 2257, 2291, 2336, 2418, 2503, 2541};
constexpr std::uint8_t OwnCtcss = 51;
// The standard DCS codes, octal digits as written.
constexpr std::uint16_t Dcs[] = {023, 025, 026, 031, 032, 036, 043, 047, 051, 053, 054, 065, 071, 072, 073, 074,
    0114, 0115, 0116, 0122, 0125, 0131, 0132, 0134, 0143, 0145, 0152, 0155, 0156, 0162, 0165, 0172, 0174, 0205, 0212,
    0223, 0225, 0226, 0243, 0244, 0245, 0246, 0251, 0252, 0255, 0261, 0263, 0265, 0266, 0271, 0274, 0306, 0311, 0315,
    0325, 0331, 0332, 0343, 0346, 0351, 0356, 0364, 0365, 0371, 0411, 0412, 0413, 0423, 0431, 0432, 0445, 0446, 0452,
    0454, 0455, 0462, 0464, 0465, 0466, 0503, 0506, 0516, 0523, 0526, 0532, 0546, 0565, 0606, 0612, 0624, 0627, 0631,
    0632, 0654, 0662, 0664, 0703, 0712, 0723, 0731, 0732, 0734, 0743, 0754};
// A tone choice: off, the CTCSS tones, the DCS codes normal then
// inverted, the channel's own CTCSS tone.
constexpr std::int32_t ToneOff = 0;
constexpr std::int32_t FirstCtcss = 1;
constexpr std::int32_t FirstDcs = FirstCtcss + OwnCtcss;
constexpr std::int32_t FirstInvertedDcs = FirstDcs + static_cast<std::int32_t>(std::size(Dcs));
constexpr std::int32_t ToneOwn = FirstInvertedDcs + static_cast<std::int32_t>(std::size(Dcs));
constexpr std::int16_t ToneChoices = ToneOwn + 1;

constexpr Field ChannelFields[] = {
    text("Name", "Basic", 0x23, 16),
    frequency("RX", "Basic", 0x00, Codec::RxFrequency),
    frequency("TX", "Basic", 0x04, Codec::TxFrequency),
    choice("Mode", "Basic", 0x08, 0, 2, Modes),
    choice("Power", "Basic", 0x08, 2, 2, Powers),
    choice("Bandwidth", "Basic", 0x08, 4, 1, Bandwidths),
    onOff("RX only", "Basic", 0x09, 5),
    onOff("Reverse", "Basic", 0x09, 4),
    bits("Colour code", "DMR", 0x20, 0, 4, 15),
    choice("Slot", "DMR", 0x21, 0, 1, Slots),
    link("Contact", "DMR", 0x14, Book::Contacts, Codec::Index32),
    link("Radio ID", "DMR", 0x18, Book::RadioIds),
    link("RX group", "DMR", 0x1C, Book::GroupLists),
    onOff("Roaming", "DMR", 0x34, 2, true),
    choice("Admit", "DMR", 0x1A, 0, 2, Admits),
    onOff("Talkaround", "DMR", 0x09, 7),
    onOff("Call confirm", "DMR", 0x09, 6),
    onOff("SMS", "DMR", 0x3B, 2, true),
    onOff("SMS confirm", "DMR", 0x21, 1),
    onOff("Data ACK", "DMR", 0x34, 3, true),
    onOff("Talker alias", "DMR", 0x3B, 4),
    onOff("Simplex TDMA", "DMR", 0x21, 2),
    onOff("Through mode", "DMR", 0x34, 1),
    onOff("Lone worker", "DMR", 0x21, 7),
    special("RX tone", "FM", FieldKind::Choice, Codec::RxTone, 0, ToneChoices - 1),
    special("TX tone", "FM", FieldKind::Choice, Codec::TxTone, 0, ToneChoices - 1),
    choice("Squelch", "FM", 0x19, 4, 3, SquelchModes),
    choice("FM admit", "FM", 0x1A, 0, 2, FmAdmits),
    choice("PTT ID", "FM", 0x19, 0, 2, PttIds),
    onOff("Auto scan", "FM", 0x34, 4),
    choice("APRS TX", "APRS", 0x35, 0, 8, AprsKinds),
    choice("FM APRS PTT", "APRS", 0x36, 0, 8, AprsPtts),
    onOff("DMR APRS PTT", "APRS", 0x37),
    number("DMR APRS sys", "APRS", 0x38, 0, 7, 1, 1),
    number("FM APRS freq", "APRS", 0x3C, 0, 7, 1, 1),
};
constexpr Field ZoneFields[] = {
    text("Name", "Basic", 0x00, 16),
};
constexpr Field ContactFields[] = {
    text("Name", "Basic", 0x01, 16),
    readOnly(choice("Type", "Basic", 0x00, 0, 8, CallTypes)),
    readOnly(special("Number", "Basic", FieldKind::Number, Codec::BcdNumber, 0x23, 0)),
    choice("Alert", "Basic", 0x27, 0, 8, AlertTypes),
};
constexpr Field RepeaterFields[] = {
    text("Name", "Basic", 0x0A, 16),
    frequency("RX", "Basic", 0x00),
    frequency("TX", "Basic", 0x04),
    number("Colour code", "Basic", 0x08, 0, 16),
    choice("Slot", "Basic", 0x09, 0, 8, Slots),
};
constexpr Field RoamZoneFields[] = {
    text("Name", "Basic", 0x40, 16),
};
constexpr Field RadioIdFields[] = {
    special("ID", "Basic", FieldKind::Number, Codec::BcdNumber, 0x00, 0),
    text("Name", "Basic", 0x05, 16),
};
constexpr Field SettingsFields[] = {
    number("TX timeout", "Radio", 0x04, 0, 255, 30, 0, " s", true),
    number("Squelch A", "Radio", 0x09, 0, 5),
    number("Squelch B", "Radio", 0x0A, 0, 5),
    choice("Battery save", "Radio", 0x0B, 0, 8, BatterySaves),
    choice("Light time", "Radio", 0x27, 0, 8, LightTimes),
    onOff("GPS", "Radio", 0x28),
    special("Time zone", "Radio", FieldKind::Choice, Codec::TimeZone, 0x30, 52),
    onOff("Voice enhance", "Radio", 0x57),
    choice("Main side", "Radio", 0x2C, 0, 8, Sides),
    choice("Side A", "Radio", 0x15, 0, 8, VfoModes),
    choice("Side B", "Radio", 0x16, 0, 8, VfoModes),
    link("Zone A", "Radio", 0x1F, Book::Zones),
    link("Zone B", "Radio", 0x20, Book::Zones),
    link("Priority A", "Radio", 0x6F, Book::Zones),
    link("Priority B", "Radio", 0x70, Book::Zones),
    choice("Auto off", "Radio", 0x03, 0, 8, AutoOffs),
    choice("VFO step", "Radio", 0x08, 0, 8, VfoSteps),
    onOff("Sub channel", "Radio", 0x2D),
    choice("TBST", "Radio", 0x2E, 0, 8, Tbsts),
    choice("Units", "Radio", 0xBD, 0, 8, Units),
    choice("Shows", "Display", 0x01, 0, 8, Shows),
    number("Brightness", "Display", 0x26, 0, 4, 1, 1),
    choice("Language", "Display", 0x05, 0, 8, Languages),
    onOff("Show clock", "Display", 0x51),
    onOff("Show call", "Display", 0xAF),
    onOff("Show contact", "Display", 0xB9),
    onOff("Channel no.", "Display", 0xB8),
    onOff("Last heard", "Display", 0xC2),
    choice("Last caller", "Display", 0x4D, 0, 8, Callers),
    onOff("Volume shown", "Display", 0x47),
    onOff("Call end note", "Display", 0x3A),
    number("TX light", "Display", 0xE1, 0, 255, 1, 0, " s", true),
    choice("Call colour", "Display", 0xBC, 0, 8, NameColours),
    choice("Name colour", "Display", 0xE4, 0, 8, NameColours),
    choice("Text colour", "Display", 0xC0, 0, 8, TextColours),
    onOff("Key tone", "Audio", 0x00),
    number("Key volume", "Audio", 0xBB, 0, 15),
    number("Max volume", "Audio", 0x3B, 0, 8),
    number("Max headphone", "Audio", 0x52, 0, 8),
    number("Mic gain", "Audio", 0x0F, 0, 4, 1, 1),
    number("VOX level", "Audio", 0x0C, 0, 3, 1, 0, "", true),
    onOff("Boot tone", "Audio", 0x39),
    onOff("SMS tone", "Audio", 0x29),
    onOff("Call alert", "Audio", 0x2F),
    choice("Talk permit", "Audio", 0x31, 0, 2, TalkPermits),
    onOff("Reset tone", "Audio", 0x32),
    onOff("Idle tone", "Audio", 0x36),
    onOff("Recording", "Audio", 0x22),
    number("Mute delay", "Audio", 0xE9, 0, 254, 1, 1, " min"),
    number("Group hang", "DMR", 0x19, 0, 30, 1, 0, " s"),
    number("Private hang", "DMR", 0x1A, 0, 30, 1, 0, " s"),
    number("Preamble", "DMR", 0x1B, 0, 255, 20, 0, " ms"),
    number("Wake header", "DMR", 0x1C, 0, 255, 20, 0, " ms"),
    onOff("Filter own ID", "DMR", 0x38),
    choice("Slot match", "DMR", 0x49, 0, 8, SlotMatches),
    onOff("CC match", "DMR", 0x4A),
    onOff("ID match", "DMR", 0x4B),
    onOff("Slot hold", "DMR", 0x4C),
    onOff("Keep caller", "DMR", 0xE3),
    choice("SMS format", "DMR", 0xC3, 0, 8, SmsFormats),
    choice("Boot screen", "Boot", 0x06, 0, 8, BootDisplays),
    onOff("Password", "Boot", 0x07),
    onOff("Boot channels", "Boot", 0xD7),
    link("Boot zone A", "Boot", 0xD8, Book::Zones),
    link("Boot zone B", "Boot", 0xD9, Book::Zones),
    onOff("GPS check", "Boot", 0xEB),
    onOff("Factory reset", "Boot", 0xEC),
    onOff("Auto lock", "Keys", 0x02),
    number("Menu exit", "Keys", 0x37, 0, 11, 5, 1, " s"),
    onOff("Lock knob", "Keys", 0xBE, 0),
    onOff("Lock keypad", "Keys", 0xBE, 1),
    onOff("Lock side keys", "Keys", 0xBE, 3),
    onOff("Forced lock", "Keys", 0xBE, 4),
    special("PF1 short", "Keys", FieldKind::Number, Codec::KeyCode, 0x10, 255),
    special("PF2 short", "Keys", FieldKind::Number, Codec::KeyCode, 0x11, 255),
    special("PF3 short", "Keys", FieldKind::Number, Codec::KeyCode, 0x12, 255),
    special("PF1 long", "Keys", FieldKind::Number, Codec::KeyCode, 0x41, 255),
    special("PF2 long", "Keys", FieldKind::Number, Codec::KeyCode, 0x42, 255),
    special("PF3 long", "Keys", FieldKind::Number, Codec::KeyCode, 0x43, 255),
    special("P1 short", "Keys", FieldKind::Number, Codec::KeyCode, 0x13, 255),
    special("P2 short", "Keys", FieldKind::Number, Codec::KeyCode, 0x14, 255),
    special("P1 long", "Keys", FieldKind::Number, Codec::KeyCode, 0x44, 255),
    special("P2 long", "Keys", FieldKind::Number, Codec::KeyCode, 0x45, 255),
    number("Long press", "Keys", 0x46, 0, 4, 1, 1, " s"),
    link("Roam zone", "Roaming", 0xDC, Book::RoamZones),
    onOff("Auto roam", "Roaming", 0xE7),
    choice("Roam start", "Roaming", 0xE0, 0, 8, RoamStarts),
    number("Roam period", "Roaming", 0xBA, 0, 255, 1, 0, " min"),
    number("Quiet time", "Roaming", 0xBF, 0, 255, 1, 0, " s"),
    onOff("Range check", "Roaming", 0xDD),
    number("Check every", "Roaming", 0xDE, 0, 9, 5, 1, " s"),
    number("Check tries", "Roaming", 0xDF, 0, 2, 1, 3),
    onOff("Out of range", "Roaming", 0xE5),
    number("Notices", "Roaming", 0xEA, 0, 9, 1, 1),
};
constexpr Field MoreSettingsFields[] = {
    choice("GPS systems", "Radio", 0x35, 0, 8, GpsSystems),
    onOff("GPS roaming", "Radio", 0x44),
    onOff("ATPC", "Radio", 0x3C),
    onOff("TOT warning", "Radio", 0x3B),
    choice("Date format", "Radio", 0x42, 0, 8, DateFormats),
    number("FM mic gain", "Radio", 0x43, 0, 4, 1, 1),
    onOff("FM idle tone", "Radio", 0x41),
    onOff("Send alias", "DMR", 0x00),
    choice("Alias shown", "DMR", 0x1E, 0, 8, AliasShows),
    choice("Alias coding", "DMR", 0x1F, 0, 8, AliasCodings),
    onOff("Show type", "Display", 0x40, 0),
    onOff("Show slot", "Display", 0x40, 1),
    onOff("Show CC", "Display", 0x40, 2),
    choice("Ch. B colour", "Display", 0x39, 0, 8, NameColours),
    choice("Zone A colour", "Display", 0x3D, 0, 8, NameColours),
    choice("Zone B colour", "Display", 0x3E, 0, 8, NameColours),
};
constexpr Field AprsFields[] = {
    text("Callsign", "APRS", 0x1D, 6, true),
    number("SSID", "APRS", 0x23, 0, 15),
    text("Path", "APRS", 0x24, 20),
    text("Symbol table", "APRS", 0x39, 1),
    text("Symbol", "APRS", 0x3A, 1),
    number("Beacon every", "APRS", 0x0B, 0, 255, 30, 0, " s", true),
    choice("Power", "APRS", 0x3B, 0, 2, Powers),
    choice("Bandwidth", "APRS", 0xAA, 0, 8, Bandwidths),
    frequency("Frequency", "APRS", 0xAC),
    number("TX delay", "APRS", 0x05, 0, 255, 20, 0, " ms"),
    number("Prewave", "APRS", 0x3C, 0, 255, 10, 0, " ms"),
    number("Manual every", "APRS", 0x0A, 0, 255, 1, 0, " s"),
    text("Destination", "APRS", 0x16, 6, true),
    number("Dest. SSID", "APRS", 0x1C, 0, 15),
    onOff("Pass all", "Receive", 0xAB),
    onOff("Positions", "Receive", 0xA8, 0),
    onOff("Mic-E", "Receive", 0xA8, 1),
    onOff("Objects", "Receive", 0xA8, 2),
    onOff("Items", "Receive", 0xA8, 3),
    onOff("Messages", "Receive", 0xA8, 4),
    onOff("Weather", "Receive", 0xA8, 5),
    onOff("NMEA", "Receive", 0xA8, 6),
    onOff("Status", "Receive", 0xA8, 7),
    onOff("Other", "Receive", 0xA9, 0),
    choice("Position", "Fixed", 0x0D, 0, 8, PositionSources),
    number("Lat. degrees", "Fixed", 0x0E, 0, 90),
    number("Lat. minutes", "Fixed", 0x0F, 0, 59),
    number("Lat. seconds", "Fixed", 0x10, 0, 59),
    choice("Latitude", "Fixed", 0x11, 0, 8, NorthSouth),
    number("Lon. degrees", "Fixed", 0x12, 0, 180),
    number("Lon. minutes", "Fixed", 0x13, 0, 59),
    number("Lon. seconds", "Fixed", 0x14, 0, 59),
    choice("Longitude", "Fixed", 0x15, 0, 8, EastWest),
    frequency("Frequency 2", "More freqs", 0xB0),
    frequency("Frequency 3", "More freqs", 0xB4),
    frequency("Frequency 4", "More freqs", 0xB8),
    frequency("Frequency 5", "More freqs", 0xBC),
    frequency("Frequency 6", "More freqs", 0xC0),
    frequency("Frequency 7", "More freqs", 0xC4),
    frequency("Frequency 8", "More freqs", 0xC8),
};
constexpr Field AprsTextFields[] = {
    text("Text", "APRS", 0x00, AprsSettings::CommentBytes),
};

// Where each book's records are and what they hold.
struct Layout
{
    std::uint32_t base;
    std::uint32_t stride;
    std::uint32_t perBank; // 0: one run of records
    std::uint32_t bankStride;
    std::uint16_t bytes;
    std::uint32_t bitmap; // 0: a single record
    bool inverted;        // a clear bit marks a record in use
    std::uint16_t capacity;
    std::uint16_t nameAt;
    std::uint8_t nameBytes; // 0: titled by number
    const char *title;
    const Field *fields;
    std::size_t fieldCount;
};

template <std::size_t N>
constexpr Layout layout(std::uint32_t base, std::uint32_t stride, std::uint32_t perBank, std::uint32_t bankStride,
    std::uint16_t bytes, std::uint32_t bitmap, bool inverted, std::uint16_t capacity, std::uint16_t nameAt,
    std::uint8_t nameBytes, const char *title, const Field (&fields)[N])
{
    return {base, stride, perBank, bankStride, bytes, bitmap, inverted, capacity, nameAt, nameBytes, title, fields, N};
}

constexpr Layout Layouts[] = {
    layout(ChannelBanks, ChannelBytes, ChannelsPerBank, ChannelBankStride, ChannelBytes, ChannelBitmap, false,
        CodeplugSource::MaxChannels, 0x23, 16, "Channel", ChannelFields),
    layout(ZoneNames, ZoneNameStride, 0, 0, ZoneNameStride, ZoneBitmap, false, CodeplugSource::MaxZones, 0, 16, "Zone",
        ZoneFields),
    layout(ContactBanks, ContactBytes, ContactsPerBank, ContactBankStride, ContactBytes, ContactBitmap, true,
        MaxContacts, 1, 16, "Contact", ContactFields),
    layout(GroupLists, GroupListStride, 0, 0, GroupListStride, GroupListBitmap, false, MaxGroupLists, 0, 0,
        "Group list", ZoneFields),
    layout(RoamingRepeaters, RoamingRepeaterBytes, 0, 0, RoamingRepeaterBytes, RoamingRepeaterBitmap, false,
        MaxRoamingRepeaters, RoamingRepeaterName, 16, "Repeater", RepeaterFields),
    layout(RoamingZones, RoamingZoneBytes, 0, 0, RoamingZoneBytes, RoamingZoneBitmap, false,
        CodeplugSource::MaxRoamingZones, RoamingZoneName, 16, "Roam zone", RoamZoneFields),
    layout(RadioIds, RadioIdBytes, 0, 0, RadioIdBytes, RadioIdBitmap, false, MaxRadioIds, 0x05, 16, "Radio ID",
        RadioIdFields),
    layout(Settings, 0, 0, 0, SettingsBytes, 0, false, 1, 0, 0, "Settings", SettingsFields),
    layout(SettingsExtension, 0, 0, 0, SettingsExtensionEditBytes, 0, false, 1, 0, 0, "More settings",
        MoreSettingsFields),
    layout(AprsSettingsBlock, 0, 0, 0, AprsSettingsBytes, 0, false, 1, 0, 0, "APRS", AprsFields),
    layout(AprsComment, 0, 0, 0, AprsSettings::CommentBytes, 0, false, 1, 0, 0, "APRS text", AprsTextFields),
};
static_assert(std::size(Layouts) == static_cast<std::size_t>(Book::Count));

const Layout &of(Book book)
{
    return Layouts[static_cast<std::size_t>(book)];
}

// Group lists list no named field: only their bytes.
std::size_t namedFields(Book book)
{
    return book == Book::GroupLists ? 0 : of(book).fieldCount;
}

std::uint32_t le32(const std::uint8_t *p)
{
    return static_cast<std::uint32_t>(p[0] | p[1] << 8 | p[2] << 16) | static_cast<std::uint32_t>(p[3]) << 24;
}
void putLe32(std::uint8_t *p, std::uint32_t v)
{
    for (int i = 0; i < 4; ++i) {
        p[i] = static_cast<std::uint8_t>(v >> (8 * i));
    }
}
std::uint16_t le16(const std::uint8_t *p)
{
    return static_cast<std::uint16_t>(p[0] | p[1] << 8);
}
void putLe16(std::uint8_t *p, std::uint16_t v)
{
    p[0] = static_cast<std::uint8_t>(v);
    p[1] = static_cast<std::uint8_t>(v >> 8);
}
// Eight BCD digits, big-endian; -1 when a nibble is not a digit.
std::int64_t bcd(const std::uint8_t *p)
{
    std::int64_t v = 0;
    for (int i = 0; i < 4; ++i) {
        for (const int digit : {p[i] >> 4, p[i] & 0xF}) {
            if (digit > 9) {
                return -1;
            }
            v = v * 10 + digit;
        }
    }
    return v;
}
void putBcd(std::uint8_t *p, std::uint32_t v)
{
    for (int i = 3; i >= 0; --i) {
        const unsigned low = v % 10;
        v /= 10;
        const unsigned high = v % 10;
        v /= 10;
        p[i] = static_cast<std::uint8_t>(high << 4 | low);
    }
}

std::uint32_t mask(const Field &f)
{
    return f.bits >= 32 ? 0xFFFFFFFFu : (1u << f.bits) - 1;
}

// A channel's tone as a tone choice.
std::int32_t toneOf(const std::uint8_t *r, bool transmit)
{
    const unsigned kind = transmit ? (r[0x09] >> 2) & 3 : r[0x09] & 3;
    if (kind == 1) {
        const std::uint8_t index = transmit ? r[0x0A] : r[0x0B];
        return index < OwnCtcss ? FirstCtcss + index : index == OwnCtcss ? ToneOwn
                                                                         : ToneOff;
    }
    if (kind == 2) {
        const std::uint16_t stored = le16(&r[transmit ? 0x0C : 0x0E]);
        const std::uint16_t code = stored % 512;
        for (std::size_t i = 0; i < std::size(Dcs); ++i) {
            if (Dcs[i] == code) {
                return (stored >= 512 ? FirstInvertedDcs : FirstDcs) + static_cast<std::int32_t>(i);
            }
        }
    }
    return ToneOff;
}
void setTone(std::uint8_t *r, bool transmit, std::int32_t v)
{
    unsigned kind = 0;
    if (v >= FirstCtcss && v < FirstDcs) {
        kind = 1;
        r[transmit ? 0x0A : 0x0B] = static_cast<std::uint8_t>(v - FirstCtcss);
    } else if (v == ToneOwn) {
        kind = 1;
        r[transmit ? 0x0A : 0x0B] = OwnCtcss;
    } else if (v >= FirstDcs && v < ToneOwn) {
        kind = 2;
        const bool inverted = v >= FirstInvertedDcs;
        const std::uint16_t code = Dcs[v - (inverted ? FirstInvertedDcs : FirstDcs)];
        putLe16(&r[transmit ? 0x0C : 0x0E], static_cast<std::uint16_t>(code + (inverted ? 512 : 0)));
    }
    const unsigned shift = transmit ? 2 : 0;
    r[0x09] = static_cast<std::uint8_t>((r[0x09] & ~(3u << shift)) | kind << shift);
}

// A channel's transmit frequency from its receive frequency, offset and
// direction.
std::int64_t transmitOf(const std::uint8_t *r)
{
    const std::int64_t rx = bcd(&r[0x00]);
    const std::int64_t offset = bcd(&r[0x04]);
    if (rx < 0 || offset < 0) {
        return -1;
    }
    switch ((r[0x08] >> 6) & 3) {
    case 1:
        return (rx + offset) * 10;
    case 2:
        return (rx - offset) * 10;
    default:
        return rx * 10;
    }
}
void setTransmit(std::uint8_t *r, std::uint32_t hz)
{
    const std::int64_t stored = bcd(&r[0x00]);
    const std::uint32_t rx = stored < 0 ? 0 : static_cast<std::uint32_t>(stored) * 10;
    const unsigned direction = hz > rx ? 1 : hz < rx ? 2
                                                     : 0;
    putBcd(&r[0x04], (hz > rx ? hz - rx : rx - hz) / 10);
    r[0x08] = static_cast<std::uint8_t>((r[0x08] & 0x3F) | direction << 6);
}

// Writes @p parts into @p out, zero-terminated.
template <class... Parts>
void put(plug::Text out, const Parts &...parts)
{
    lib::TextWriter w(out.first(plug::TextBytes - 1));
    (w << ... << parts);
    out[w.size()] = '\0';
}

void showFrequency(plug::Text out, std::int64_t hz)
{
    if (hz < 0) {
        put(out, "-");
        return;
    }
    const auto h = static_cast<std::uint32_t>(hz);
    put(out, h / 1'000'000, '.', lib::zeroPadded(h % 1'000'000 / 10, 5));
}

void showTone(plug::Text out, std::int32_t v)
{
    if (v >= FirstCtcss && v < FirstDcs) {
        const std::uint16_t t = Ctcss[v - FirstCtcss];
        put(out, t / 10, '.', static_cast<char>('0' + t % 10), " Hz");
    } else if (v >= FirstDcs && v < ToneOwn) {
        const bool inverted = v >= FirstInvertedDcs;
        const std::uint16_t code = Dcs[v - (inverted ? FirstInvertedDcs : FirstDcs)];
        put(out, 'D', static_cast<char>('0' + (code >> 6 & 7)), static_cast<char>('0' + (code >> 3 & 7)),
            static_cast<char>('0' + (code & 7)), inverted ? 'I' : 'N');
    } else if (v == ToneOwn) {
        put(out, "Own tone");
    } else {
        put(out, "Off");
    }
}

} // namespace

std::size_t D878Uv2PlugEditor::capacity(Book book) const
{
    return of(book).capacity;
}

std::uint32_t D878Uv2PlugEditor::address(Book book, std::size_t index) const
{
    const Layout &l = of(book);
    const auto i = static_cast<std::uint32_t>(index);
    if (l.perBank != 0) {
        return l.base + i / l.perBank * l.bankStride + i % l.perBank * l.stride;
    }
    return l.base + i * l.stride;
}

bool D878Uv2PlugEditor::inUse(Book book, std::size_t index)
{
    const Layout &l = of(book);
    if (index >= l.capacity) {
        return false;
    }
    if (l.bitmap == 0) {
        return true;
    }
    std::uint8_t byte = 0;
    if (!readable(m_codeplug.read(l.bitmap + static_cast<std::uint32_t>(index / 8), std::span(&byte, 1)))) {
        return false;
    }
    return ((byte >> (index % 8) & 1) != 0) != l.inverted;
}

std::ptrdiff_t D878Uv2PlugEditor::nearest(Book book, std::ptrdiff_t from, int direction)
{
    const Layout &l = of(book);
    const auto cap = static_cast<std::ptrdiff_t>(l.capacity);
    // A chunk of the bitmap at a time.
    std::uint8_t chunk[32];
    std::ptrdiff_t chunkFirst = -1;
    for (std::ptrdiff_t i = from; i >= 0 && i < cap; i += direction) {
        if (l.bitmap == 0) {
            return i;
        }
        const std::ptrdiff_t byte = i / 8;
        if (chunkFirst < 0 || byte < chunkFirst || byte >= chunkFirst + static_cast<std::ptrdiff_t>(sizeof chunk)) {
            chunkFirst = direction > 0 ? byte : std::max<std::ptrdiff_t>(0, byte - static_cast<std::ptrdiff_t>(sizeof chunk) + 1);
            if (!readable(m_codeplug.read(l.bitmap + static_cast<std::uint32_t>(chunkFirst), chunk))) {
                return -1;
            }
        }
        if (((chunk[byte - chunkFirst] >> (i % 8) & 1) != 0) != l.inverted) {
            return i;
        }
    }
    return -1;
}

std::size_t D878Uv2PlugEditor::count(Book book)
{
    std::size_t n = 0;
    for (std::ptrdiff_t i = nearest(book, 0, 1); i >= 0; i = nearest(book, i + 1, 1)) {
        ++n;
    }
    return n;
}

bool D878Uv2PlugEditor::title(Book book, std::size_t index, plug::Text out)
{
    const Layout &l = of(book);
    if (l.bitmap == 0) {
        put(out, l.title);
        return true;
    }
    if (l.nameBytes == 0) {
        if (book == Book::RadioIds) {
            std::uint8_t id[4];
            if (readable(m_codeplug.read(address(book, index), id)) && bcd(id) >= 0) {
                put(out, "ID ", static_cast<std::uint32_t>(bcd(id)));
                return true;
            }
        }
        put(out, l.title, ' ', index + 1);
        return true;
    }
    char name[17] = {};
    if (!readable(m_codeplug.read(address(book, index) + l.nameAt,
            std::span(reinterpret_cast<std::uint8_t *>(name), l.nameBytes)))) {
        return false;
    }
    std::size_t n = 0;
    while (n < l.nameBytes && name[n] != '\0' && static_cast<unsigned char>(name[n]) != 0xFF) {
        ++n;
    }
    if (n == 0) {
        put(out, l.title, ' ', index + 1);
    } else {
        put(out, std::string_view(name, n));
    }
    return true;
}

bool D878Uv2PlugEditor::open(Book book, std::size_t index)
{
    const Layout &l = of(book);
    if (!inUse(book, index) || l.bytes > MaxRecordBytes) {
        return false;
    }
    if (!readable(m_codeplug.read(address(book, index), std::span(m_read, l.bytes)))) {
        return false;
    }
    std::memcpy(m_record, m_read, l.bytes);
    m_book = book;
    m_index = index;
    m_bytes = l.bytes;
    m_open = true;
    return true;
}

std::size_t D878Uv2PlugEditor::namedCount() const
{
    return namedFields(m_book);
}

const D878Uv2PlugEditor::Field *D878Uv2PlugEditor::named(std::size_t i) const
{
    return i < namedCount() ? &of(m_book).fields[i] : nullptr;
}

std::size_t D878Uv2PlugEditor::fields() const
{
    return m_open ? namedCount() + m_bytes : 0;
}

plug::FieldInfo D878Uv2PlugEditor::field(std::size_t i) const
{
    FieldInfo info;
    if (const Field *f = named(i)) {
        info.page = f->page;
        info.kind = f->kind;
        info.min = f->min;
        info.max = f->kind == FieldKind::Link ? static_cast<std::int32_t>(of(f->link).capacity) - 1 : f->max;
        info.maxChars = f->kind == FieldKind::Text ? f->bits : 0;
        info.link = f->link;
        info.readOnly = f->readOnly;
        if (f->kind == FieldKind::Frequency) {
            info.min = 0;
            info.max = 999'999'990;
            info.step = 10;
        }
        return info;
    }
    info.page = "Bytes";
    info.kind = FieldKind::Byte;
    info.max = 255;
    return info;
}

void D878Uv2PlugEditor::label(std::size_t i, plug::Text out) const
{
    if (const Field *f = named(i)) {
        put(out, f->label);
    } else {
        put(out, "Byte ", lib::hex(static_cast<std::uint32_t>(i - namedCount()), 2));
    }
}

std::int32_t D878Uv2PlugEditor::valueIn(std::size_t i, const std::uint8_t *r) const
{
    const Field *f = named(i);
    if (f == nullptr) {
        return i - namedCount() < m_bytes ? r[i - namedCount()] : 0;
    }
    switch (f->codec) {
    case Codec::Bits:
    case Codec::TimeZone:
    case Codec::KeyCode:
        return static_cast<std::int32_t>((r[f->at] >> f->shift) & mask(*f));
    case Codec::NotBit:
        return (r[f->at] >> f->shift & 1) == 0 ? 1 : 0;
    case Codec::Frequency:
    case Codec::RxFrequency: {
        const std::int64_t v = bcd(&r[f->at]);
        return v < 0 ? -1 : static_cast<std::int32_t>(v * 10);
    }
    case Codec::TxFrequency:
        return static_cast<std::int32_t>(transmitOf(r));
    case Codec::BcdNumber: {
        const std::int64_t v = bcd(&r[f->at]);
        return v < 0 ? -1 : static_cast<std::int32_t>(v);
    }
    case Codec::Index32: {
        const std::uint32_t v = le32(&r[f->at]);
        return v >= of(f->link).capacity ? -1 : static_cast<std::int32_t>(v);
    }
    case Codec::IndexByte:
        return r[f->at] >= of(f->link).capacity ? -1 : r[f->at];
    case Codec::RxTone:
        return toneOf(r, false);
    case Codec::TxTone:
        return toneOf(r, true);
    case Codec::Text:
    case Codec::PaddedText:
        return 0;
    }
    return 0;
}

bool D878Uv2PlugEditor::setValue(std::size_t i, std::int32_t v)
{
    if (!m_open) {
        return false;
    }
    const Field *f = named(i);
    if (f == nullptr) {
        if (i - namedCount() >= m_bytes) {
            return false;
        }
        m_record[i - namedCount()] = static_cast<std::uint8_t>(std::clamp<std::int32_t>(v, 0, 255));
        return true;
    }
    if (f->readOnly) {
        return false;
    }
    std::uint8_t *r = m_record;
    switch (f->codec) {
    case Codec::Bits:
    case Codec::TimeZone:
    case Codec::KeyCode: {
        const std::uint32_t m = mask(*f) << f->shift;
        const auto raw = static_cast<std::uint32_t>(std::clamp<std::int32_t>(v, f->min, f->max));
        r[f->at] = static_cast<std::uint8_t>((r[f->at] & ~m) | ((raw << f->shift) & m));
        return true;
    }
    case Codec::NotBit:
        r[f->at] = static_cast<std::uint8_t>(v != 0 ? r[f->at] & ~(1u << f->shift) : r[f->at] | 1u << f->shift);
        return true;
    case Codec::Frequency:
        putBcd(&r[f->at], static_cast<std::uint32_t>(std::max<std::int32_t>(v, 0)) / 10);
        return true;
    case Codec::RxFrequency: {
        const std::int64_t tx = transmitOf(r);
        putBcd(&r[f->at], static_cast<std::uint32_t>(std::max<std::int32_t>(v, 0)) / 10);
        if (tx >= 0) {
            setTransmit(r, static_cast<std::uint32_t>(tx));
        }
        return true;
    }
    case Codec::TxFrequency:
        setTransmit(r, static_cast<std::uint32_t>(std::max<std::int32_t>(v, 0)));
        return true;
    case Codec::BcdNumber:
        putBcd(&r[f->at], static_cast<std::uint32_t>(std::clamp<std::int32_t>(v, 0, 99'999'999)));
        return true;
    case Codec::Index32:
        putLe32(&r[f->at], v < 0 ? 0xFFFFFFFFu : static_cast<std::uint32_t>(v));
        return true;
    case Codec::IndexByte:
        r[f->at] = v < 0 ? 0xFF : static_cast<std::uint8_t>(v);
        return true;
    case Codec::RxTone:
    case Codec::TxTone:
        setTone(r, f->codec == Codec::TxTone, std::clamp<std::int32_t>(v, 0, ToneChoices - 1));
        return true;
    case Codec::Text:
    case Codec::PaddedText:
        return false;
    }
    return false;
}

bool D878Uv2PlugEditor::setText(std::size_t i, std::string_view text)
{
    const Field *f = named(i);
    if (!m_open || f == nullptr || f->readOnly || f->kind != FieldKind::Text) {
        return false;
    }
    const std::uint8_t fill = f->codec == Codec::PaddedText ? ' ' : 0;
    for (std::size_t k = 0; k < f->bits; ++k) {
        m_record[f->at + k] = k < text.size() ? static_cast<std::uint8_t>(text[k]) : fill;
    }
    return true;
}

void D878Uv2PlugEditor::textIn(std::size_t i, const std::uint8_t *r, plug::Text out) const
{
    const Field *f = named(i);
    std::size_t n = 0;
    if (f != nullptr) {
        while (n < f->bits && n + 1 < plug::TextBytes && r[f->at + n] != 0 && r[f->at + n] != 0xFF) {
            out[n] = static_cast<char>(r[f->at + n]);
            ++n;
        }
        if (f->codec == Codec::PaddedText) {
            while (n > 0 && out[n - 1] == ' ') {
                --n;
            }
        }
    }
    out[n] = '\0';
}

void D878Uv2PlugEditor::show(std::size_t i, plug::Text out, bool original)
{
    const std::uint8_t *r = original ? m_read : m_record;
    showIn(i, valueIn(i, r), r, out);
}

void D878Uv2PlugEditor::showValue(std::size_t i, std::int32_t v, plug::Text out)
{
    showIn(i, v, m_record, out);
}

void D878Uv2PlugEditor::showIn(std::size_t i, std::int32_t v, const std::uint8_t *r, plug::Text out)
{
    const Field *f = named(i);
    if (f == nullptr) {
        put(out, lib::hex(static_cast<std::uint32_t>(v), 2), "  ", v);
        return;
    }
    switch (f->codec) {
    case Codec::Text:
    case Codec::PaddedText:
        textIn(i, r, out);
        return;
    case Codec::Frequency:
    case Codec::RxFrequency:
    case Codec::TxFrequency:
        showFrequency(out, v);
        return;
    case Codec::RxTone:
    case Codec::TxTone:
        showTone(out, v);
        return;
    case Codec::TimeZone: {
        const int minutes = (v - 24) * 30;
        const int a = minutes < 0 ? -minutes : minutes;
        put(out, "UTC", minutes < 0 ? '-' : '+', lib::zeroPadded(static_cast<std::uint32_t>(a / 60), 2), ':',
            lib::zeroPadded(static_cast<std::uint32_t>(a % 60), 2));
        return;
    }
    case Codec::KeyCode:
        if (v >= 0 && static_cast<std::size_t>(v) < std::size(KeyFunctions)) {
            put(out, KeyFunctions[v]);
        } else {
            put(out, "Code ", lib::hex(static_cast<std::uint32_t>(v), 2));
        }
        return;
    case Codec::Index32:
    case Codec::IndexByte:
        if (v < 0) {
            put(out, "None");
        } else if (!inUse(f->link, static_cast<std::size_t>(v))) {
            put(out, '#', v + 1, ", unused");
        } else if (!title(f->link, static_cast<std::size_t>(v), out)) {
            put(out, '#', v + 1);
        }
        return;
    case Codec::BcdNumber:
        if (v < 0) {
            put(out, "-");
        } else {
            put(out, v);
        }
        return;
    case Codec::Bits:
    case Codec::NotBit:
        break;
    }
    if (f->kind == FieldKind::Choice) {
        put(out, f->names != nullptr && v >= 0 && v <= f->max ? f->names[v] : "?");
    } else if (f->offAtZero && v == 0) {
        put(out, "Off");
    } else {
        put(out, (v + f->bias) * f->scale, f->unit);
    }
}

std::size_t D878Uv2PlugEditor::choices(std::size_t i) const
{
    const Field *f = named(i);
    return f != nullptr && f->kind == FieldKind::Choice ? static_cast<std::size_t>(f->max + 1) : 0;
}

bool D878Uv2PlugEditor::changed(std::size_t i) const
{
    const Field *f = named(i);
    if (f == nullptr) {
        const std::size_t b = i - namedCount();
        return b < m_bytes && m_record[b] != m_read[b];
    }
    if (f->kind == FieldKind::Text) {
        return std::memcmp(&m_record[f->at], &m_read[f->at], f->bits) != 0;
    }
    return valueIn(i, m_record) != valueIn(i, m_read);
}

// The bytes a field's value is kept in.
bool D878Uv2PlugEditor::holds(const Field &f, std::size_t byte) const
{
    const auto within = [byte](std::size_t at, std::size_t n) { return byte >= at && byte < at + n; };
    switch (f.codec) {
    case Codec::Bits:
    case Codec::NotBit:
    case Codec::TimeZone:
    case Codec::KeyCode:
    case Codec::IndexByte:
        return within(f.at, 1);
    case Codec::Frequency:
    case Codec::BcdNumber:
    case Codec::Index32:
        return within(f.at, 4);
    case Codec::RxFrequency:
        return within(0x00, 9);
    case Codec::TxFrequency:
        return within(0x04, 5);
    case Codec::Text:
    case Codec::PaddedText:
        return within(f.at, f.bits);
    case Codec::RxTone:
        return within(0x09, 1) || within(0x0B, 1) || within(0x0E, 2);
    case Codec::TxTone:
        return within(0x09, 1) || within(0x0A, 1) || within(0x0C, 2);
    }
    return false;
}

bool D878Uv2PlugEditor::listed(std::size_t i) const
{
    if (!changed(i)) {
        return false;
    }
    if (named(i) != nullptr) {
        return true;
    }
    const std::size_t byte = i - namedCount();
    for (std::size_t k = 0; k < namedCount(); ++k) {
        if (changed(k) && holds(*named(k), byte)) {
            return false;
        }
    }
    return true;
}

std::size_t D878Uv2PlugEditor::changes() const
{
    std::size_t n = 0;
    for (std::size_t i = 0; i < fields(); ++i) {
        n += listed(i) ? 1 : 0;
    }
    return n;
}

void D878Uv2PlugEditor::revert()
{
    std::memcpy(m_record, m_read, m_bytes);
}

plug::SaveResult D878Uv2PlugEditor::save()
{
    if (!m_open) {
        return SaveResult::Failed;
    }
    if (std::memcmp(m_record, m_read, m_bytes) == 0) {
        return SaveResult::Unchanged;
    }
    const SaveResult r = m_write(address(m_book, m_index), std::span<const std::uint8_t>(m_record, m_bytes));
    if (r == SaveResult::Saved) {
        std::memcpy(m_read, m_record, m_bytes);
    }
    return r;
}

namespace {

// The next word of @p text as a hexadecimal number.
bool hexWord(std::string_view &text, std::uint32_t &value)
{
    while (!text.empty() && text.front() == ' ') {
        text.remove_prefix(1);
    }
    if (text.size() >= 2 && text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) {
        text.remove_prefix(2);
    }
    std::size_t n = 0;
    value = 0;
    for (; n < text.size() && text[n] != ' '; ++n) {
        const char c = text[n];
        const int digit = c >= '0' && c <= '9' ? c - '0'
            : c >= 'a' && c <= 'f'             ? c - 'a' + 10
            : c >= 'A' && c <= 'F'             ? c - 'A' + 10
                                               : -1;
        if (digit < 0 || n >= 8) {
            return false;
        }
        value = value << 4 | static_cast<std::uint32_t>(digit);
    }
    text.remove_prefix(n);
    return n > 0;
}

} // namespace

bool D878Uv2PlugEditor::debugCommand(std::string_view text)
{
    std::uint32_t address = 0;
    if (text.starts_with("peek ")) {
        text.remove_prefix(5);
        std::uint32_t count = 16;
        if (!hexWord(text, address) || (!text.empty() && !hexWord(text, count)) || count == 0 || count > 0x100) {
            log::info("plug") << "peek <address> [count up to 100], in hex";
            return true;
        }
        std::uint8_t line[16];
        for (std::uint32_t at = 0; at < count; at += sizeof line) {
            const std::size_t n = std::min<std::size_t>(sizeof line, count - at);
            if (!readable(m_codeplug.read(address + at, std::span(line, n)))) {
                log::info("plug") << "peek " << lib::hex(address + at) << ": cannot read";
                return true;
            }
            auto out = log::info("plug");
            out << lib::hex(address + at) << ':';
            for (std::size_t i = 0; i < n; ++i) {
                out << ' ' << lib::hex(line[i], 2);
            }
        }
        return true;
    }
    if (text.starts_with("poke ")) {
        text.remove_prefix(5);
        std::uint8_t bytes[32];
        std::size_t n = 0;
        bool ok = hexWord(text, address);
        std::uint32_t b = 0;
        while (ok && !text.empty() && n < sizeof bytes) {
            ok = hexWord(text, b) && b <= 0xFF;
            bytes[n++] = static_cast<std::uint8_t>(b);
        }
        if (!ok || n == 0 || !text.empty()) {
            log::info("plug") << "poke <address> <byte>..., up to 32 bytes in hex";
            return true;
        }
        const SaveResult r = m_write(address, std::span<const std::uint8_t>(bytes, n));
        log::info("plug") << "poke " << lib::hex(address) << ", " << n << " bytes: "
                          << (r == SaveResult::Saved         ? "written"
                                     : r == SaveResult::Busy ? "busy, the PC is writing"
                                                             : "refused or failed");
        return true;
    }
    return false;
}

} // namespace dmm::services
