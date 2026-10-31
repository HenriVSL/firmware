// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Henri Lavikainen

#pragma once

#include "dmm/lib/fn_ref.h"
#include "dmm/services/codeplug.h"
#include "dmm/services/d878uv2_codeplug.h"
#include "dmm/services/d878uv2_preferences.h"
#include "dmm/services/plug_editor.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace dmm::services {

/// Keeps settings changed on the radio in their places in the D878UVII's
/// codeplug, and the radio's own preferences in their record. Values come
/// in as they change; once none has changed for QuietMs, the bytes that
/// differ from the codeplug are written together, so a run of changes
/// costs one write and an unchanged value none.
class D878Uv2SettingsStore
{
public:
    using SaveResult = plug::SaveResult;
    /// Bytes to write at a codeplug address.
    struct Piece
    {
        std::uint32_t address;
        std::span<const std::uint8_t> data;
    };
    /// Writes the pieces, in rising address order, as one write.
    using Writer = lib::FnRef<SaveResult(std::span<const Piece>)>;

    static constexpr std::uint32_t QuietMs = 3000;

    D878Uv2SettingsStore(D878Uv2Codeplug &codeplug, Writer write)
        : m_codeplug(codeplug), m_write(write)
    {
    }

    /// The general settings the radio changes: squelch, key lock, battery
    /// save, backlight, GPS, time zone and the roaming zone.
    void keepSettings(const CodeplugSettings &s, std::uint32_t nowMs);
    /// The APRS beacon interval and power.
    void keepAprs(std::uint16_t beaconIntervalS, PowerLevel power, std::uint32_t nowMs);
    void keepPreferences(const RadioPreferences &p, std::uint32_t nowMs);
    /// Side A's or B's zone, and its position in the zone's channel list.
    void keepZone(bool sideB, std::uint8_t zone, std::uint16_t position, std::uint32_t nowMs);
    /// Channel @p index's power and squelch.
    void keepChannel(std::uint16_t index, PowerLevel power, SquelchMode squelch, std::uint32_t nowMs);

    bool pending() const { return m_pending; }
    /// Writes what is kept once QuietMs have passed since the last change.
    /// Unchanged when nothing was due or nothing differed; after Busy or
    /// Failed the values stay kept and are tried again QuietMs later.
    SaveResult flush(std::uint32_t nowMs);

private:
    struct Zone
    {
        std::uint8_t zone;
        std::uint16_t position;
    };
    struct ChannelValues
    {
        std::uint16_t index;
        PowerLevel power;
        SquelchMode squelch;
    };
    void changed(std::uint32_t nowMs);
    bool addPiece(std::uint32_t address, std::span<const std::uint8_t> data);

    D878Uv2Codeplug &m_codeplug;
    Writer m_write;
    bool m_pending = false;
    std::uint32_t m_changedAt = 0;
    std::optional<CodeplugSettings> m_settings;
    std::optional<std::uint16_t> m_aprsInterval;
    PowerLevel m_aprsPower = PowerLevel::Low;
    std::optional<RadioPreferences> m_preferences;
    std::optional<Zone> m_zone[2];
    std::optional<ChannelValues> m_channel;

    // The bytes being written; pieces point into them.
    std::uint8_t m_settingsBytes[0x100] = {};
    std::uint8_t m_extension[1] = {};
    std::uint8_t m_selection[2][2] = {};
    std::uint8_t m_aprs[2] = {};
    std::uint8_t m_preferenceBytes[d878uv2::PreferencesRecordBytes] = {};
    std::uint8_t m_channelBytes[0x40] = {};
    static constexpr std::size_t MaxPieces = 8;
    Piece m_pieces[MaxPieces] = {};
    std::size_t m_pieceCount = 0;
};

} // namespace dmm::services
