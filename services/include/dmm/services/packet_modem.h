// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Henri Lavikainen

#pragma once

#include "dmm/services/rtty_modulator.h"

#include <concepts>
#include <cstdint>
#include <span>

namespace dmm::services {

/// A modem that sends packet radio frames (1200 baud AFSK) and RTTY as
/// audio into the transmitter. The caller keys the transmitter around a
/// send.
///
/// - send(frame, leadMs, levelPercent): starts sending @p frame (from the
///   first address to the frame check sequence) after @p leadMs of flags,
///   at @p levelPercent of the board's full audio level; false if a frame
///   is still going out.
/// - sending(): true until the last flag has been sent.
/// - sendRtty(codes, config, leadMs, tailMs, levelPercent): starts sending
///   @p codes (5-bit ITA2, unchanged until sent) as RTTY with @p leadMs of
///   mark before and @p tailMs after, at @p levelPercent of the board's
///   full audio level; false if something is still going out.
/// - stop(): ends what is going out at once.
/// - SampleRateHz: the rate the modem makes its audio at.
template <typename T>
concept PacketModem = requires(T &m, std::span<const std::uint8_t> frame, std::uint16_t leadMs, std::uint8_t levelPercent,
    const RttyModulator::Config &config) {
    { m.send(frame, leadMs, levelPercent) } -> std::same_as<bool>;
    { m.sending() } -> std::same_as<bool>;
    { m.sendRtty(frame, config, leadMs, leadMs, levelPercent) } -> std::same_as<bool>;
    { m.stop() };
    { T::SampleRateHz } -> std::convertible_to<std::uint32_t>;
};

} // namespace dmm::services
