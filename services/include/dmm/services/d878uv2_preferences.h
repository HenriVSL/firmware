// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Henri Lavikainen

#pragma once

#include "dmm/services/codeplug.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace dmm::services::d878uv2 {

/// The radio's own preferences as kept in the D878UVII's codeplug: the mark
/// "DMMP", a version, the fields, and a CRC-32 of the rest in the last four
/// bytes. Later versions add fields after the earlier ones.
inline constexpr std::size_t PreferencesRecordBytes = 0x40;

/// The record's preferences; empty when it holds none (no mark, or a bad
/// checksum).
std::optional<RadioPreferences> decodePreferences(std::span<const std::uint8_t, PreferencesRecordBytes> record);

/// @p record filled with @p p.
void encodePreferences(const RadioPreferences &p, std::span<std::uint8_t, PreferencesRecordBytes> record);

} // namespace dmm::services::d878uv2
