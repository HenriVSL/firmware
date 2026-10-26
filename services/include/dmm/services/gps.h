// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Henri Lavikainen

#pragma once

#include "dmm/lib/fn_ref.h"

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace dmm::services {

/// Satellite systems a GPS receiver uses, as bits.
namespace gps_systems {
inline constexpr std::uint8_t Gps = 1;
inline constexpr std::uint8_t Beidou = 2;
inline constexpr std::uint8_t Glonass = 4;
inline constexpr std::uint8_t All = Gps | Beidou | Glonass;

/// "GPS+BDS" and the like, at most 7 characters; "?" for none.
constexpr std::string_view name(std::uint8_t systems)
{
    constexpr std::string_view Names[] = {"?", "GPS", "BDS", "GPS+BDS", "GLO", "GPS+GLO", "BDS+GLO", "All"};
    return systems <= All ? Names[systems] : Names[0];
}
} // namespace gps_systems

/// Sends bytes to the receiver over its link.
using GpsWrite = lib::FnRef<void(std::span<const std::uint8_t>)>;

/// The serial link to a GPS receiver: NMEA sentences in, commands out; the
/// board names the type (dmm/board/parts.h). The receiver's supply is
/// switched elsewhere (FmTransceiver::setGpsPower). Used by one task.
///
/// - start(): opens the link; drops anything received before.
/// - stop(): closes the link and leaves it so it does not feed an
///   unpowered receiver.
/// - read(data): takes up to data.size() received bytes; returns how many.
/// - write(data): sends @p data to the receiver.
/// - lostBytes(): received bytes lost so far because they came faster
///   than they were read.
template <class T>
concept GpsPort = requires(T &port, const T &state, std::span<std::uint8_t> in, std::span<const std::uint8_t> out) {
    port.start();
    port.stop();
    { port.read(in) } -> std::same_as<std::size_t>;
    port.write(out);
    { state.lostBytes() } -> std::same_as<std::uint32_t>;
};

/// What the receiver chip understands beyond NMEA output; the board names
/// the type (dmm/board/parts.h).
///
/// - configure(systems, write): asks the receiver for what DeltaMM reads
///   (position, fix and satellites once a second) from the satellite
///   systems in @p systems (gps_systems bits), through @p write.
/// - debugCommand(text, write), debugHelp(): the chip's own console
///   commands, for experiments. False if @p text is none of them.
template <class T>
concept GpsReceiver = requires(T &receiver, const T &state, std::uint8_t systems, GpsWrite write, std::string_view text) {
    receiver.configure(systems, write);
    { receiver.debugCommand(text, write) } -> std::same_as<bool>;
    { state.debugHelp() } -> std::same_as<const char *>;
};

} // namespace dmm::services
