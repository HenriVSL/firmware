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
/// bytes. Later versions add fields after the earlier ones; a byte out of
/// range (0xFF in a record from before its field) reads as the default.
///
/// | Bytes | Field |
/// | --- | --- |
/// | 0-3 | "DMMP" |
/// | 4 | version |
/// | 5-8 | frequency offset, Hz, signed, little-endian |
/// | 9 | flags: clock UTC, APRS receive, packets on the speaker |
/// | 10-15 | the home screen's rows |
/// | 16-17 | microphone gain (feat/mic-gain) |
/// | 18 | the dedicated mode on |
/// | 19 | the text mode's kind (0: RTTY) |
/// | 20-23 | the text mode's frequency, Hz, little-endian; 0 or all ones: not set |
/// | 24 | its tone pair (0: 2125/2295, 1: 1275/1445) |
/// | 25 | its speed (0: 45.45, 1: 50, 2: 75 Bd) |
/// | 26 | its options: 1 reverse, 2 no unshift on space, 4 on the speaker |
/// | 60-63 | CRC-32 of bytes 0-59 |
inline constexpr std::size_t PreferencesRecordBytes = 0x40;

/// The record's preferences; empty when it holds none (no mark, or a bad
/// checksum).
std::optional<RadioPreferences> decodePreferences(std::span<const std::uint8_t, PreferencesRecordBytes> record);

/// @p record filled with @p p.
void encodePreferences(const RadioPreferences &p, std::span<std::uint8_t, PreferencesRecordBytes> record);

} // namespace dmm::services::d878uv2
