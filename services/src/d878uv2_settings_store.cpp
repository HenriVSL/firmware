// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Henri Lavikainen

#include "dmm/services/d878uv2_settings_store.h"

#include "d878uv2_layout.h"

#include <algorithm>
#include <iterator>

namespace dmm::services {

using namespace d878uv2;

namespace {

// Light times by their codeplug index, in seconds.
constexpr std::uint16_t LightTimes[] = {0, 5, 10, 15, 20, 25, 30, 60, 120, 180, 240, 300, 900, 1800, 2700, 3600};

std::uint8_t clampTo(int v, int low, int high)
{
    return static_cast<std::uint8_t>(v < low ? low : v > high ? high
                                                              : v);
}

// The general settings' bytes for the values the radio changes; the
// other bytes stay as they are.
void encodeSettings(const CodeplugSettings &s, std::span<std::uint8_t, 0x100> b)
{
    b[0x09] = clampTo(s.squelchA, 0, 5);
    b[0x0A] = clampTo(s.squelchB, 0, 5);
    b[0xBE] = static_cast<std::uint8_t>((b[0xBE] & ~0x1Bu) | (s.keyLock.knob ? 0x01 : 0) |
        (s.keyLock.keypad ? 0x02 : 0) | (s.keyLock.side ? 0x08 : 0) | (s.keyLock.forced ? 0x10 : 0));
    // Auto key lock follows the menu exit time: 5 s steps from 5 s.
    b[0x02] = s.keyLock.autoLockMs != 0 ? 1 : 0;
    if (s.keyLock.autoLockMs != 0) {
        b[0x37] = clampTo(static_cast<int>((s.keyLock.autoLockMs + 2500) / 5000) - 1, 0, 11);
    }
    b[0x0B] = clampTo(s.batterySave, 0, 2);
    const auto light = std::find(std::begin(LightTimes), std::end(LightTimes), s.backlightS);
    if (light != std::end(LightTimes)) {
        b[0x27] = static_cast<std::uint8_t>(light - std::begin(LightTimes));
    }
    b[0x26] = clampTo(s.backlightLevel - 1, 0, 4);
    b[0x28] = s.gps ? 1 : 0;
    // The time zone in half hours from UTC-12:00 (0).
    b[0x30] = clampTo(s.timeZoneMinutes / 30 + 24, 0, 52);
    b[0xDC] = s.roaming.zone;
}

} // namespace

void D878Uv2SettingsStore::changed(std::uint32_t nowMs)
{
    m_pending = true;
    m_changedAt = nowMs;
}

void D878Uv2SettingsStore::keepSettings(const CodeplugSettings &s, std::uint32_t nowMs)
{
    m_settings = s;
    changed(nowMs);
}

void D878Uv2SettingsStore::keepAprs(std::uint16_t beaconIntervalS, PowerLevel power, std::uint32_t nowMs)
{
    m_aprsInterval = beaconIntervalS;
    m_aprsPower = power;
    changed(nowMs);
}

void D878Uv2SettingsStore::keepPreferences(const RadioPreferences &p, std::uint32_t nowMs)
{
    m_preferences = p;
    changed(nowMs);
}

void D878Uv2SettingsStore::keepZone(bool sideB, std::uint8_t zone, std::uint16_t position, std::uint32_t nowMs)
{
    m_zone[sideB ? 1 : 0] = Zone{zone, position};
    changed(nowMs);
}

void D878Uv2SettingsStore::keepChannel(std::uint16_t index, PowerLevel power, SquelchMode squelch, std::uint32_t nowMs)
{
    // Kept for one channel at a time: a change on another channel writes
    // the first one's at once.
    if (m_channel && m_channel->index != index) {
        flush(m_changedAt + QuietMs);
    }
    m_channel = ChannelValues{index, power, squelch};
    changed(nowMs);
}

bool D878Uv2SettingsStore::addPiece(std::uint32_t address, std::span<const std::uint8_t> data)
{
    if (m_pieceCount >= MaxPieces) {
        return false;
    }
    m_pieces[m_pieceCount++] = Piece{address, data};
    return true;
}

D878Uv2SettingsStore::SaveResult D878Uv2SettingsStore::flush(std::uint32_t nowMs)
{
    if (!m_pending || static_cast<std::int32_t>(nowMs - m_changedAt) < static_cast<std::int32_t>(QuietMs)) {
        return SaveResult::Unchanged;
    }
    m_pieceCount = 0;
    bool unreadable = false;
    // Reads @p size bytes at @p address into @p into; false if unreadable.
    auto readInto = [&](std::uint32_t address, std::span<std::uint8_t> into) {
        if (!readable(m_codeplug.read(address, into))) {
            unreadable = true;
            return false;
        }
        return true;
    };

    if (m_settings || m_zone[0] || m_zone[1]) {
        std::uint8_t read[sizeof m_settingsBytes];
        if (readInto(Settings, read)) {
            std::copy(std::begin(read), std::end(read), std::begin(m_settingsBytes));
            if (m_settings) {
                encodeSettings(*m_settings, m_settingsBytes);
            }
            for (int side = 0; side < 2; ++side) {
                if (m_zone[side]) {
                    m_settingsBytes[side == 0 ? 0x1F : 0x20] = m_zone[side]->zone;
                }
            }
            if (!std::equal(std::begin(read), std::end(read), std::begin(m_settingsBytes))) {
                addPiece(Settings, m_settingsBytes);
            }
        }
    }
    for (int side = 0; side < 2; ++side) {
        if (!m_zone[side]) {
            continue;
        }
        const std::uint32_t at = ZoneSelections + (side == 1 ? ZoneSelectionSideB : 0) + m_zone[side]->zone * 2u;
        std::uint8_t read[2];
        if (readInto(at, read)) {
            m_selection[side][0] = static_cast<std::uint8_t>(m_zone[side]->position);
            m_selection[side][1] = static_cast<std::uint8_t>(m_zone[side]->position >> 8);
            if (read[0] != m_selection[side][0] || read[1] != m_selection[side][1]) {
                addPiece(at, m_selection[side]);
            }
        }
    }
    if (m_aprsInterval) {
        constexpr std::uint32_t IntervalAt = AprsSettingsBlock + 0x0B; // in 30 s steps
        constexpr std::uint32_t PowerAt = AprsSettingsBlock + 0x3B;
        std::uint8_t interval = 0;
        std::uint8_t power = 0;
        if (readInto(IntervalAt, std::span(&interval, 1)) && readInto(PowerAt, std::span(&power, 1))) {
            m_aprs[0] = clampTo((*m_aprsInterval + 15) / 30, 0, 255);
            m_aprs[1] = static_cast<std::uint8_t>((power & ~3u) | (static_cast<unsigned>(m_aprsPower) & 3u));
            if (interval != m_aprs[0]) {
                addPiece(IntervalAt, std::span(&m_aprs[0], 1));
            }
            if (power != m_aprs[1]) {
                addPiece(PowerAt, std::span(&m_aprs[1], 1));
            }
        }
    }
    if (m_settings) {
        // The satellite systems: the mode 0..6 is one less than the bits.
        constexpr std::uint32_t SystemsAt = SettingsExtension + 0x35;
        std::uint8_t read = 0;
        if (readInto(SystemsAt, std::span(&read, 1))) {
            m_extension[0] = clampTo(m_settings->gpsSystems - 1, 0, 6);
            if (read != m_extension[0]) {
                addPiece(SystemsAt, m_extension);
            }
        }
    }
    if (m_preferences) {
        std::uint8_t read[PreferencesRecordBytes];
        if (readInto(Preferences, read)) {
            encodePreferences(*m_preferences, m_preferenceBytes);
            if (!std::equal(std::begin(read), std::end(read), std::begin(m_preferenceBytes))) {
                addPiece(Preferences, m_preferenceBytes);
            }
        }
    }
    if (m_channel) {
        const std::uint32_t at =
            ChannelBanks + (m_channel->index / ChannelsPerBank) * ChannelBankStride + (m_channel->index % ChannelsPerBank) * ChannelBytes;
        std::uint8_t read[sizeof m_channelBytes];
        if (readInto(at, read)) {
            std::copy(std::begin(read), std::end(read), std::begin(m_channelBytes));
            m_channelBytes[0x08] =
                static_cast<std::uint8_t>((read[0x08] & ~0x0Cu) | (static_cast<unsigned>(m_channel->power) & 3u) << 2);
            // Squelch at bits 4..6: 0 carrier, 1 the receive tone.
            m_channelBytes[0x19] = static_cast<std::uint8_t>(
                (read[0x19] & ~0x70u) | (m_channel->squelch == SquelchMode::Tone ? 0x10u : 0u));
            if (!std::equal(std::begin(read), std::end(read), std::begin(m_channelBytes))) {
                addPiece(at, m_channelBytes);
            }
        }
    }

    if (unreadable) {
        m_changedAt = nowMs;
        return SaveResult::Failed;
    }
    if (m_pieceCount == 0) {
        m_pending = false;
        return SaveResult::Unchanged;
    }
    std::sort(m_pieces, m_pieces + m_pieceCount, [](const Piece &a, const Piece &b) { return a.address < b.address; });
    const SaveResult r = m_write(std::span<const Piece>(m_pieces, m_pieceCount));
    if (r == SaveResult::Saved) {
        m_pending = false;
        m_settings.reset();
        m_aprsInterval.reset();
        m_preferences.reset();
        m_zone[0].reset();
        m_zone[1].reset();
        m_channel.reset();
    } else {
        m_changedAt = nowMs;
    }
    return r;
}

} // namespace dmm::services
