// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Henri Lavikainen

#pragma once

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <span>

namespace dmm::services {

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

} // namespace dmm::services
