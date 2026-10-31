// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Henri Lavikainen

#include "../drivers/fake_spi_nand.h"
#include "dmm/services/d878uv2_codeplug.h"
#include "dmm/services/d878uv2_preferences.h"
#include "dmm/services/d878uv2_settings_store.h"
#include "dmm/services/d878uv2_storage.h"

#include <doctest/doctest.h>

#include <array>
#include <cstdint>
#include <functional>
#include <vector>

using dmm::drivers::storage::SpiNand;
using dmm::services::CodeplugSettings;
using dmm::services::D878Uv2Codeplug;
using dmm::services::D878Uv2SettingsStore;
using dmm::services::D878Uv2Storage;
using dmm::services::PowerLevel;
using dmm::services::RadioPreferences;
using dmm::services::SquelchMode;
using SaveResult = dmm::services::plug::SaveResult;

namespace {

void noDelay(unsigned) {}

constexpr std::uint32_t ValidMarker = 0x1FFF8;
constexpr std::uint32_t Settings = 0x02500000;
constexpr std::uint32_t Channels = 0x00800000;

// A codeplug with general settings, APRS settings and one channel; the
// store's writer puts the pieces straight into the valid copies.
struct Radio
{
    FakeSpiNand chip;
    SpiNand nand{chip, noDelay};
    D878Uv2Storage storage{nand};
    D878Uv2Codeplug codeplug{storage};
    std::vector<std::vector<std::uint32_t>> writes;
    SaveResult answer = SaveResult::Saved;
    std::function<SaveResult(std::span<const D878Uv2SettingsStore::Piece>)> writeFn =
        [this](std::span<const D878Uv2SettingsStore::Piece> pieces) {
            if (answer != SaveResult::Saved) {
                return answer;
            }
            std::vector<std::uint32_t> addresses;
            for (const auto &p : pieces) {
                chip.put(p.address, std::vector<std::uint8_t>(p.data.begin(), p.data.end()));
                addresses.push_back(p.address);
            }
            writes.push_back(addresses);
            codeplug.forgetCopies();
            return SaveResult::Saved;
        };
    D878Uv2SettingsStore store{codeplug, writeFn};

    Radio()
    {
        auto markValid = [&](std::uint32_t block) {
            chip.put(block + ValidMarker, {0xFF, 0xFF, 0xFF, 0xFF, 0x55, 0x55, 0xAA, 0xAA});
        };
        markValid(Settings);
        std::vector<std::uint8_t> s(0x100, 0);
        s[0x09] = 1;
        s[0x0A] = 1;
        s[0x26] = 3; // brightness 4
        s[0x30] = 24;
        s[0xBE] = 0x40; // a bit the store leaves alone
        s[0xDC] = 0;
        chip.put(Settings, s);
        std::vector<std::uint8_t> aprs(0x100, 0);
        aprs[0x0B] = 0;
        aprs[0x3B] = 0x80; // power low, with a high bit to keep
        chip.put(Settings + 0x1000, aprs);
        chip.put(Settings + 0x1400, std::vector<std::uint8_t>(0x40, 0));
        markValid(Channels);
        std::vector<std::uint8_t> ch(0x40, 0);
        ch[0] = 0x43; // 434.50000 MHz
        ch[1] = 0x45;
        ch[8] = 0x01; // digital, low power
        ch[0x0B] = 9; // receive CTCSS 88.5 Hz, so the tone squelch shows
        ch[9] = 1;
        chip.put(Channels, ch);
        storage.start();
    }
};

} // namespace

TEST_CASE("kept settings are written only after a quiet time, in their places")
{
    Radio r;
    const auto first = r.codeplug.settings();
    REQUIRE(first.has_value());
    CodeplugSettings s = *first;
    s.squelchA = 4;
    s.backlightLevel = 2;
    s.backlightS = 60;
    s.gps = true;
    s.gpsSystems = 7;
    s.timeZoneMinutes = 180;
    s.batterySave = 1;
    s.keyLock.knob = true;
    s.roaming.zone = 2;
    r.store.keepSettings(s, 1000);
    CHECK(r.store.pending());
    CHECK(r.store.flush(1000 + D878Uv2SettingsStore::QuietMs - 1) == SaveResult::Unchanged);
    CHECK(r.writes.empty());
    CHECK(r.store.flush(1000 + D878Uv2SettingsStore::QuietMs) == SaveResult::Saved);
    CHECK_FALSE(r.store.pending());
    REQUIRE(r.writes.size() == 1);
    CHECK(r.writes[0] == std::vector<std::uint32_t>{Settings, Settings + 0x1435});

    const auto read = r.codeplug.settings();
    REQUIRE(read.has_value());
    const CodeplugSettings t = *read;
    CHECK(t.squelchA == 4);
    CHECK(t.squelchB == 1);
    CHECK(t.backlightLevel == 2);
    CHECK(t.backlightS == 60);
    CHECK(t.gps);
    CHECK(t.gpsSystems == 7);
    CHECK(t.timeZoneMinutes == 180);
    CHECK(t.batterySave == 1);
    CHECK(t.keyLock.knob);
    CHECK(t.roaming.zone == 2);
    std::uint8_t lock = 0;
    r.codeplug.read(Settings + 0xBE, std::span(&lock, 1));
    CHECK((lock & 0x40) != 0);
}

TEST_CASE("an unchanged value writes nothing")
{
    Radio r;
    r.store.keepSettings(*r.codeplug.settings(), 0);
    CHECK(r.store.flush(D878Uv2SettingsStore::QuietMs) == SaveResult::Unchanged);
    CHECK(r.writes.empty());
    CHECK_FALSE(r.store.pending());
}

TEST_CASE("preferences are kept in their own record")
{
    Radio r;
    CHECK_FALSE(r.codeplug.preferences().has_value());
    RadioPreferences p;
    p.frequencyOffsetHz = -2000;
    p.bigContactName = true;
    p.clockUtc = true;
    p.aprsReceive = false;
    r.store.keepPreferences(p, 0);
    CHECK(r.store.flush(D878Uv2SettingsStore::QuietMs) == SaveResult::Saved);
    const auto q = r.codeplug.preferences();
    REQUIRE(q.has_value());
    CHECK(q->frequencyOffsetHz == -2000);
    CHECK(q->bigContactName);
    CHECK_FALSE(q->volumeBesidePower);
    CHECK(q->clockUtc);
    CHECK_FALSE(q->aprsReceive);
}

TEST_CASE("a damaged preferences record counts as none")
{
    std::array<std::uint8_t, dmm::services::d878uv2::PreferencesRecordBytes> rec{};
    dmm::services::d878uv2::encodePreferences(RadioPreferences{}, rec);
    CHECK(dmm::services::d878uv2::decodePreferences(rec).has_value());
    rec[9] ^= 1;
    CHECK_FALSE(dmm::services::d878uv2::decodePreferences(rec).has_value());
}

TEST_CASE("zone, channel and APRS values go to their places in one write, in address order")
{
    Radio r;
    r.store.keepAprs(300, PowerLevel::High, 0);
    r.store.keepZone(false, 3, 5, 0);
    r.store.keepChannel(0, PowerLevel::High, SquelchMode::Tone, 0);
    CHECK(r.store.flush(D878Uv2SettingsStore::QuietMs) == SaveResult::Saved);
    REQUIRE(r.writes.size() == 1);
    CHECK(r.writes[0] ==
        std::vector<std::uint32_t>{Channels, Settings, Settings + 0x100 + 3 * 2, Settings + 0x100B, Settings + 0x103B});
    CHECK(r.codeplug.settings()->zoneA == 3);
    CHECK(r.codeplug.zoneSelection(3, false) == 5);
    const auto c = r.codeplug.channel(0);
    REQUIRE(c.has_value());
    CHECK(c->power == PowerLevel::High);
    CHECK(c->squelchMode == SquelchMode::Tone);
    std::uint8_t power = 0;
    r.codeplug.read(Settings + 0x103B, std::span(&power, 1));
    CHECK(power == 0x82);
    std::uint8_t interval = 0;
    r.codeplug.read(Settings + 0x100B, std::span(&interval, 1));
    CHECK(interval == 10);
}

TEST_CASE("a write refused while the PC writes is tried again later")
{
    Radio r;
    CodeplugSettings s = *r.codeplug.settings();
    s.squelchA = 5;
    r.store.keepSettings(s, 0);
    r.answer = SaveResult::Busy;
    CHECK(r.store.flush(D878Uv2SettingsStore::QuietMs) == SaveResult::Busy);
    CHECK(r.store.pending());
    r.answer = SaveResult::Saved;
    CHECK(r.store.flush(D878Uv2SettingsStore::QuietMs + 1) == SaveResult::Unchanged);
    CHECK(r.store.flush(2 * D878Uv2SettingsStore::QuietMs) == SaveResult::Saved);
    CHECK(r.codeplug.settings()->squelchA == 5);
}
