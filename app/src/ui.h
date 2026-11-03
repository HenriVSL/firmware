// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tommi Tauriainen

#pragma once

#include "dmm/app/app.h"
#include "dmm/services/codeplug.h"
#include "dmm/services/display.h"
#include "dmm/services/input.h"
#include "dmm/services/key_lock.h"
#include "dmm/services/nmea.h"

#include <cstdint>
#include <string_view>
#include <variant>

namespace dmm::app::ui {

struct Backlight
{
    std::uint8_t level; // 0..5
};
/// How long the backlight stays on after the last key; 0 = always on.
struct BacklightTime
{
    std::uint16_t seconds;
};
struct ShowTestPattern
{
};
struct ShowLog
{
};
struct ShowKeyTest
{
};
/// Between the key test and the log.
struct ToggleView
{
};
struct PanelPower
{
    bool on;
};
/// The debounced keys held now.
struct Keys
{
    services::KeyMask mask;
};
/// A key was pressed, released, held or repeated.
struct KeyEvent
{
    services::Key key;
    services::KeyDebouncer::EventType type;
};
/// The radio started (true) or stopped transmitting.
struct Transmitting
{
    bool on;
    /// A call or an FM transmission of the user's, not a repeater wake-up
    /// or a packet.
    bool call;
};
/// A DMR call is heard (active) or over: who is calling and whom.
struct DmrCall
{
    bool active;
    bool group;
    std::uint32_t source;
    std::uint32_t destination;
};
/// Roaming on the current channel: a search under way, or the repeater in
/// use (@c site -1: none yet) with its output frequency and colour code.
struct Roaming
{
    bool searching;
    std::int8_t site;
    std::uint32_t receiveHz;
    std::uint8_t colourCode;
};
/// The squelch opened (true) or closed.
struct Receiving
{
    bool on;
};
/// Shows the radio's main screen.
struct ShowStandby
{
};
/// Locks or unlocks the keys (console).
struct SetKeyLock
{
    bool locked;
};
/// Sets battery save as Menu, Battery does (console): 0 off, 1 on, 2 fast
/// wake.
struct BatterySave
{
    std::uint8_t mode;
};
/// Sets the microphone gain as Menu > Settings > Audio does, kept with
/// the preferences, and logs both (console): dB, FromCodeplug, or
/// Unchanged.
struct MicrophoneGain
{
    static constexpr std::int8_t Unchanged = -1;
    static constexpr std::int8_t FromCodeplug = -2;
    std::int8_t dmrDb;
    std::int8_t fmDb;
};
/// What the key lock covers.
struct KeyLockSettings
{
    services::KeyLockSettings settings;
};
/// Sets the frequency offset as Menu, Settings, Radio does (console): the
/// nearest step to @p hz, kept.
struct FrequencyOffset
{
    std::int32_t hz;
};
/// Logs the key lock's state and settings.
struct KeyLockReport
{
};
/// Logs what the settings store keeps and the preferences record.
struct SettingsReport
{
};
/// Runs the console command waiting for the codeplug editor (plugCommand).
struct PlugCommand
{
};
struct EncoderClicks
{
    std::int32_t clicks; // since start
};
struct EncoderRaw
{
    std::int32_t raw;
};
struct Volume
{
    std::int16_t permille; // 0..1000, -1 = no knob
};
struct Battery
{
    std::uint16_t millivolts;
    std::uint8_t level;
    bool low;
};
/// A short message for the user, up to 20 characters; the text must
/// stay valid (a string literal).
struct Notice
{
    const char *text;
};
/// The "switching off" screen.
struct SwitchingOff
{
};
/// The GPS receiver's state and latest fix.
struct Gps
{
    services::GpsFix fix;
    bool on;
    bool receiving;
    /// Enabled, but switched off between fixes to save power.
    bool asleep;
};
/// The system task applied a new rate correction to the clock.
struct ClockRate
{
};
/// The clock calibration's progress and outcome.
struct ClockCalibration
{
    enum class State : std::uint8_t
    {
        Running,
        Done,
        TooFewFixes,
        Stopped,
        GpsOff,
    };
    State state;
    std::uint8_t minutesDone;
    std::uint8_t minutes;
};

/// Whether a queued packet went out, or was given up.
struct Packet
{
    bool sent;
};
/// Turns a dedicated receive mode on (or Off), as the menu does (console).
struct SetMode
{
    services::DedicatedMode mode;
};
/// The APRS heard list changed.
struct AprsHeard
{
};
/// An APRS beacon could not be queued.
struct AprsProblem
{
    enum class Kind : std::uint8_t
    {
        NoCallsign,
        NoFrequency,
        NoFix,
        Busy,
    };
    Kind kind;
};

using Message = std::variant<Backlight, BacklightTime, ShowTestPattern, ShowLog, ShowKeyTest, ToggleView, PanelPower, Keys, KeyEvent, EncoderClicks,
    EncoderRaw, Volume, Battery, SwitchingOff, Transmitting, Receiving, DmrCall, Roaming, ShowStandby, SetKeyLock, BatterySave, MicrophoneGain, KeyLockSettings, FrequencyOffset, KeyLockReport, SettingsReport, Notice, Gps, ClockRate, ClockCalibration, PlugCommand, Packet, AprsProblem, AprsHeard, SetMode>;

/// Starts the ui task, which owns @p display.
/// @p input (may be nullptr) gives the key labels; @p plugEditor (may be
/// nullptr) edits the codeplug; @p settingsStore (may be nullptr) keeps
/// settings changed on the radio.
void start(services::Display &display, services::CodeplugSource *codeplug, const board::Input *input,
    board::PlugEditor *plugEditor, board::SettingsStore *settingsStore);

/// Queues a message for the ui task; false when the queue is full or there
/// is no display.
bool post(const Message &message);

/// True once after the codeplug editor saved a record the radio reads only
/// at start: the radio should restart.
bool takeRestart();

/// Hands a console command to the codeplug editor (its debugCommand), run
/// on the ui task; false while another is waiting or there is no editor.
bool plugCommand(std::string_view text);

} // namespace dmm::app::ui
