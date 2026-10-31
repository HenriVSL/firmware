// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Henri Lavikainen

#include "dmm/services/d878uv2_preferences.h"

#include "dmm/lib/crc32.h"

#include <algorithm>

namespace dmm::services::d878uv2 {

namespace {

constexpr std::uint8_t Mark[] = {'D', 'M', 'M', 'P'};
constexpr std::uint8_t Version = 1;
constexpr std::size_t VersionAt = 4;
constexpr std::size_t OffsetAt = 5; // frequency offset in Hz, signed, little-endian
constexpr std::size_t FlagsAt = 9;
constexpr std::uint8_t BigContactName = 0x01;
constexpr std::uint8_t VolumeBesidePower = 0x02;
constexpr std::uint8_t ClockUtc = 0x04;
constexpr std::uint8_t AprsReceive = 0x08;
constexpr std::size_t CrcAt = PreferencesRecordBytes - 4;

std::uint32_t le32(const std::uint8_t *p)
{
    return static_cast<std::uint32_t>(p[0]) | static_cast<std::uint32_t>(p[1]) << 8 |
        static_cast<std::uint32_t>(p[2]) << 16 | static_cast<std::uint32_t>(p[3]) << 24;
}

void putLe32(std::uint8_t *p, std::uint32_t v)
{
    for (int i = 0; i < 4; ++i) {
        p[i] = static_cast<std::uint8_t>(v >> (8 * i));
    }
}

} // namespace

std::optional<RadioPreferences> decodePreferences(std::span<const std::uint8_t, PreferencesRecordBytes> record)
{
    if (!std::equal(std::begin(Mark), std::end(Mark), record.begin()) || record[VersionAt] == 0 ||
        lib::crc32(record.data(), CrcAt) != le32(&record[CrcAt])) {
        return std::nullopt;
    }
    RadioPreferences p;
    p.frequencyOffsetHz = static_cast<std::int32_t>(le32(&record[OffsetAt]));
    const std::uint8_t flags = record[FlagsAt];
    p.bigContactName = (flags & BigContactName) != 0;
    p.volumeBesidePower = (flags & VolumeBesidePower) != 0;
    p.clockUtc = (flags & ClockUtc) != 0;
    p.aprsReceive = (flags & AprsReceive) != 0;
    return p;
}

void encodePreferences(const RadioPreferences &p, std::span<std::uint8_t, PreferencesRecordBytes> record)
{
    std::fill(record.begin(), record.end(), std::uint8_t{0xFF});
    std::copy(std::begin(Mark), std::end(Mark), record.begin());
    record[VersionAt] = Version;
    putLe32(&record[OffsetAt], static_cast<std::uint32_t>(p.frequencyOffsetHz));
    record[FlagsAt] = static_cast<std::uint8_t>((p.bigContactName ? BigContactName : 0) |
        (p.volumeBesidePower ? VolumeBesidePower : 0) | (p.clockUtc ? ClockUtc : 0) | (p.aprsReceive ? AprsReceive : 0));
    putLe32(&record[CrcAt], lib::crc32(record.data(), CrcAt));
}

} // namespace dmm::services::d878uv2
