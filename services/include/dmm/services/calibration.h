// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tommi Tauriainen

#pragma once

#include "dmm/services/storage.h"

#include <concepts>
#include <cstdint>
#include <optional>

namespace dmm::services {

/// What the app needs from the radio's own calibration, kept in its
/// storage. The board names the type (dmm/board/parts.h); the values the
/// radio's chips take go from the board to their services directly.
///
/// - load(storage): reads the calibration; false when storage could not
///   be read. Called once, before any other task reads values.
/// - present(): read, and the radio has been calibrated. Once true, every
///   value may be read from any task.
/// - source(): where the values were read from, for the log.
/// - batteryOffsetMv(): the battery reading's offset in millivolts.
template <class T>
concept Calibration = requires(T &cal, const T &read, Storage &storage) {
    { cal.load(storage) } -> std::same_as<bool>;
    { read.present() } -> std::same_as<bool>;
    { read.source() } -> std::same_as<const char *>;
    { read.batteryOffsetMv() } -> std::same_as<std::optional<int>>;
};

} // namespace dmm::services
