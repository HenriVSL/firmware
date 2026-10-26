// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tommi Tauriainen

#pragma once

#include "dmm/services/power_level.h"

#include <concepts>
#include <cstdint>
#include <string_view>

namespace dmm::services {

/// One FM channel.
struct FmChannel
{
    std::uint32_t receiveHz = 0;
    std::uint32_t transmitHz = 0;
    bool wide = false; // 25 kHz rather than 12.5 kHz
};

/// What the receiver hears right now, before any debouncing.
struct FmReceiveStatus
{
    bool carrier = false; // squelch decision: a signal is present
    int rssiDbm = 0;
};

/// The analog FM transceiver and the board around it. Every radio has one;
/// the board names the type (dmm/board/parts.h). Used by the radio task
/// only. Calibrated values come from the board, not through the caller.
///
/// - present(): the chip answers.
/// - start(): resets and sets the chip up: idle, microphone muted, every RF
///   switch off. False when the chip does not answer.
/// - receive(channel, squelchLevel): tunes to the channel's receive
///   frequency and receives, squelch level 1..5. Returns nullptr, or why
///   it did not.
/// - transmit(channel, level, ctcssCentiHz): transmits on the channel's
///   transmit frequency at the power level, with a CTCSS tone in 0.01 Hz
///   units (0 = none); the microphone is unmuted. Returns nullptr, or why
///   it did not transmit (then nothing was switched on).
/// - endTransmit(): power amplifier off first, then the transmit path,
///   microphone muted, idle. Safe at any time.
/// - idle(): stops receiving or transmitting; the chip stays powered.
/// - sleepReceiver(), wakeReceiver(), receiverAsleep(): battery save
///   switches the receiver and its front end off while receiving, keeping
///   the tuning; asleep until woken, or receive(), transmit() or idle().
/// - receiving(), transmitting(), status().
/// - logStatus(what): logs @p what, then what the receiver hears in the
///   chip's own figures.
/// - setMicrophoneMuted(muted).
/// - debugCommand(text), debugHelp(): the chip's own console commands, for
///   experiments; results go to the log. False if @p text is none of them.
template <class T>
concept FmTransceiver = requires(T &fm, const T &state, const FmChannel &channel, int squelchLevel, PowerLevel level,
    std::uint16_t ctcssCentiHz, bool on, std::string_view text) {
    { fm.present() } -> std::same_as<bool>;
    { fm.start() } -> std::same_as<bool>;
    { fm.receive(channel, squelchLevel) } -> std::same_as<const char *>;
    { fm.transmit(channel, level, ctcssCentiHz) } -> std::same_as<const char *>;
    fm.endTransmit();
    fm.idle();
    fm.sleepReceiver();
    fm.wakeReceiver();
    { state.receiverAsleep() } -> std::same_as<bool>;
    { state.receiving() } -> std::same_as<bool>;
    { state.transmitting() } -> std::same_as<bool>;
    { fm.status() } -> std::same_as<FmReceiveStatus>;
    fm.logStatus(text);
    fm.setMicrophoneMuted(on);
    { fm.debugCommand(text) } -> std::same_as<bool>;
    { state.debugHelp() } -> std::same_as<const char *>;
};

} // namespace dmm::services
