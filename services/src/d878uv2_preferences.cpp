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
constexpr std::uint8_t ClockUtc = 0x04;
constexpr std::uint8_t AprsReceive = 0x08;
constexpr std::uint8_t AprsAudible = 0x10;
// The home screen: top, clock, big name, middle, beside the power,
// bottom; a byte out of range (as in a record before them) is the default.
constexpr std::size_t HomeAt = 10;
// The dedicated receive mode; a byte out of range is none.
constexpr std::size_t ModeAt = 16;
constexpr std::size_t CrcAt = PreferencesRecordBytes - 4;

template <typename E>
E choice(std::uint8_t stored, E fallback)
{
    return stored < static_cast<std::uint8_t>(E::Count) ? static_cast<E>(stored) : fallback;
}

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
    p.clockUtc = (flags & ClockUtc) != 0;
    p.aprsReceive = (flags & AprsReceive) != 0;
    p.aprsAudible = (flags & AprsAudible) != 0;
    const HomeLayout d;
    p.home.top = choice(record[HomeAt], d.top);
    p.home.clock = record[HomeAt + 1] <= 1 ? record[HomeAt + 1] == 1 : d.clock;
    p.home.big = choice(record[HomeAt + 2], d.big);
    p.home.middle = choice(record[HomeAt + 3], d.middle);
    p.home.beside = choice(record[HomeAt + 4], d.beside);
    p.home.bottom = choice(record[HomeAt + 5], d.bottom);
    p.mode = choice(record[ModeAt], DedicatedMode::Off);
    return p;
}

void encodePreferences(const RadioPreferences &p, std::span<std::uint8_t, PreferencesRecordBytes> record)
{
    std::fill(record.begin(), record.end(), std::uint8_t{0xFF});
    std::copy(std::begin(Mark), std::end(Mark), record.begin());
    record[VersionAt] = Version;
    putLe32(&record[OffsetAt], static_cast<std::uint32_t>(p.frequencyOffsetHz));
    record[FlagsAt] = static_cast<std::uint8_t>((p.clockUtc ? ClockUtc : 0) | (p.aprsReceive ? AprsReceive : 0) |
        (p.aprsAudible ? AprsAudible : 0));
    record[HomeAt] = static_cast<std::uint8_t>(p.home.top);
    record[HomeAt + 1] = p.home.clock ? 1 : 0;
    record[HomeAt + 2] = static_cast<std::uint8_t>(p.home.big);
    record[HomeAt + 3] = static_cast<std::uint8_t>(p.home.middle);
    record[HomeAt + 4] = static_cast<std::uint8_t>(p.home.beside);
    record[HomeAt + 5] = static_cast<std::uint8_t>(p.home.bottom);
    record[ModeAt] = static_cast<std::uint8_t>(p.mode);
    putLe32(&record[CrcAt], lib::crc32(record.data(), CrcAt));
}

} // namespace dmm::services::d878uv2
