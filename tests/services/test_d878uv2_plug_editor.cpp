// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Henri Lavikainen

#include "../drivers/fake_spi_nand.h"
#include "dmm/services/d878uv2_codeplug.h"
#include "dmm/services/d878uv2_plug_editor.h"
#include "dmm/services/d878uv2_storage.h"

#include <doctest/doctest.h>

#include <array>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

using dmm::drivers::storage::SpiNand;
using dmm::services::D878Uv2Codeplug;
using dmm::services::D878Uv2CodeplugWriter;
using dmm::services::D878Uv2PlugEditor;
using dmm::services::D878Uv2Storage;
using dmm::services::plug::Book;
using dmm::services::plug::FieldKind;
using dmm::services::plug::SaveResult;

namespace {

void noDelay(unsigned) {}

constexpr std::uint32_t ValidMarker = 0x1FFF8;

// A channel record: receive 145.600 MHz, -600 kHz, 25 kHz, analog, high
// power, CTCSS 88.5 Hz on transmit, DCS 023 inverted on receive.
std::vector<std::uint8_t> sampleChannel()
{
    std::vector<std::uint8_t> r(64, 0);
    r[0] = 0x14;
    r[1] = 0x56;
    r[5] = 0x06;
    r[8] = (2 << 2) | 0x10 | (2 << 6);
    r[9] = 2 | (1 << 2);
    r[0x0A] = 9;
    r[0x0E] = 19;
    r[0x0F] = 0x02;
    r[0x19] = 1 << 4;
    r[0x1C] = 0xFF;
    std::memcpy(&r[0x23], "RPT 2M", 6);
    return r;
}

struct Plug
{
    FakeSpiNand chip;
    std::unique_ptr<SpiNand> nand;
    std::unique_ptr<D878Uv2Storage> storage;
    std::unique_ptr<D878Uv2Codeplug> codeplug;
    std::unique_ptr<D878Uv2Codeplug> writerCodeplug;
    std::unique_ptr<D878Uv2CodeplugWriter> writer;

    Plug()
    {
        auto markValid = [&](std::uint32_t block) {
            chip.put(block + ValidMarker, {0xFF, 0xFF, 0xFF, 0xFF, 0x55, 0x55, 0xAA, 0xAA});
        };
        markValid(0x024C0000);
        std::vector<std::uint8_t> channelBits(500, 0x00); // the codeplug writes the whole bitmap
        channelBits[0] = 0x05;                            // channels 0 and 2
        chip.put(0x024C1500, channelBits);
        markValid(0x00800000);
        chip.put(0x00800000, sampleChannel());
        auto two = sampleChannel();
        std::memcpy(&two[0x23], "Second\0", 7);
        chip.put(0x00800000 + 2 * 0x40, two);
        markValid(0x02500000);
        std::vector<std::uint8_t> settings(0x100, 0x00);
        settings[0x30] = 30; // UTC+03:00
        chip.put(0x02500000, settings);
        nand = std::make_unique<SpiNand>(chip, noDelay);
        storage = std::make_unique<D878Uv2Storage>(*nand);
        storage->start();
        codeplug = std::make_unique<D878Uv2Codeplug>(*storage);
        writerCodeplug = std::make_unique<D878Uv2Codeplug>(*storage);
        writer = std::make_unique<D878Uv2CodeplugWriter>(*storage, *writerCodeplug);
    }

    SaveResult write(std::uint32_t address, std::span<const std::uint8_t> data)
    {
        const bool ok = writer->write(address, data) && writer->finish();
        codeplug->forgetCopies();
        return ok ? SaveResult::Saved : SaveResult::Failed;
    }
};

std::string text(D878Uv2PlugEditor &e, std::size_t field, bool original = false)
{
    std::array<char, dmm::services::plug::TextBytes> out{};
    e.show(field, out, original);
    return out.data();
}

std::size_t find(D878Uv2PlugEditor &e, const char *label)
{
    std::array<char, dmm::services::plug::TextBytes> out{};
    for (std::size_t i = 0; i < e.fields(); ++i) {
        e.label(i, out);
        if (std::string(out.data()) == label) {
            return i;
        }
    }
    FAIL("no field ", label);
    return 0;
}

} // namespace

TEST_CASE("the plug editor lists the records in use with their names")
{
    Plug p;
    auto write = [&](std::uint32_t a, std::span<const std::uint8_t> d) { return p.write(a, d); };
    D878Uv2PlugEditor e(*p.codeplug, write);
    CHECK(e.capacity(Book::Channels) == 4000);
    CHECK(e.count(Book::Channels) == 2);
    CHECK(e.nearest(Book::Channels, 0, 1) == 0);
    CHECK(e.nearest(Book::Channels, 1, 1) == 2);
    CHECK(e.nearest(Book::Channels, 1, -1) == 0);
    CHECK(e.nearest(Book::Channels, 3, 1) == -1);
    std::array<char, dmm::services::plug::TextBytes> name{};
    REQUIRE(e.title(Book::Channels, 2, name));
    CHECK(std::string(name.data()) == "Second");
    REQUIRE(e.title(Book::Settings, 0, name));
    CHECK(std::string(name.data()) == "Settings");
    CHECK_FALSE(e.open(Book::Channels, 1));
}

TEST_CASE("a channel's fields read as names, frequencies and tones")
{
    Plug p;
    auto write = [&](std::uint32_t a, std::span<const std::uint8_t> d) { return p.write(a, d); };
    D878Uv2PlugEditor e(*p.codeplug, write);
    REQUIRE(e.open(Book::Channels, 0));
    CHECK(text(e, find(e, "Name")) == "RPT 2M");
    CHECK(e.value(find(e, "RX")) == 145'600'000);
    CHECK(text(e, find(e, "RX")) == "145.60000");
    CHECK(text(e, find(e, "TX")) == "145.00000");
    CHECK(text(e, find(e, "Mode")) == "FM");
    CHECK(text(e, find(e, "Power")) == "High");
    CHECK(text(e, find(e, "Bandwidth")) == "25 kHz");
    CHECK(text(e, find(e, "RX tone")) == "D023I");
    CHECK(text(e, find(e, "TX tone")) == "88.5 Hz");
    CHECK(text(e, find(e, "RX group")) == "None");
    CHECK(e.field(find(e, "TX tone")).kind == FieldKind::Choice);
    CHECK(e.choices(find(e, "TX tone")) == 261);
    // Every byte follows the named fields.
    CHECK(e.fields() == find(e, "Squelch") + 1 + 64);
    CHECK(e.field(e.fields() - 1).kind == FieldKind::Byte);
    CHECK(e.changes() == 0);
}

TEST_CASE("changing the receive frequency keeps the transmit frequency")
{
    Plug p;
    auto write = [&](std::uint32_t a, std::span<const std::uint8_t> d) { return p.write(a, d); };
    D878Uv2PlugEditor e(*p.codeplug, write);
    REQUIRE(e.open(Book::Channels, 0));
    const std::size_t rx = find(e, "RX");
    const std::size_t tx = find(e, "TX");
    REQUIRE(e.setValue(rx, 145'750'000));
    CHECK(e.value(tx) == 145'000'000);
    CHECK(e.changed(rx));
    CHECK_FALSE(e.changed(tx));
    REQUIRE(e.setValue(tx, 145'750'000));
    CHECK(e.value(tx) == 145'750'000);
    REQUIRE(e.setValue(tx, 146'350'000));
    CHECK(e.value(tx) == 146'350'000);
    CHECK(text(e, rx, true) == "145.60000");
    e.revert();
    CHECK(e.changes() == 0);
}

TEST_CASE("tones, choices, text and bytes change in the record")
{
    Plug p;
    auto write = [&](std::uint32_t a, std::span<const std::uint8_t> d) { return p.write(a, d); };
    D878Uv2PlugEditor e(*p.codeplug, write);
    REQUIRE(e.open(Book::Channels, 0));
    const std::size_t tone = find(e, "RX tone");
    REQUIRE(e.setValue(tone, 1 + 12)); // CTCSS 97.4 Hz
    CHECK(text(e, tone) == "97.4 Hz");
    REQUIRE(e.setValue(tone, 0));
    CHECK(text(e, tone) == "Off");
    REQUIRE(e.setValue(find(e, "Power"), 0));
    CHECK(text(e, find(e, "Power")) == "Low");
    REQUIRE(e.setText(find(e, "Name"), "Local"));
    CHECK(text(e, find(e, "Name")) == "Local");
    const std::size_t byte0 = find(e, "Squelch") + 1;
    CHECK(text(e, byte0) == "14  20");
    CHECK(e.value(find(e, "Roaming")) == 1);
    REQUIRE(e.setValue(find(e, "Roaming"), 0));
    CHECK(e.changed(find(e, "Roaming")));
    CHECK(e.changes() >= 4);
}

TEST_CASE("a saved record reads back from the codeplug")
{
    Plug p;
    auto write = [&](std::uint32_t a, std::span<const std::uint8_t> d) { return p.write(a, d); };
    D878Uv2PlugEditor e(*p.codeplug, write);
    REQUIRE(e.open(Book::Channels, 2));
    CHECK(e.save() == SaveResult::Unchanged);
    REQUIRE(e.setText(find(e, "Name"), "Edited"));
    REQUIRE(e.setValue(find(e, "Power"), 3));
    CHECK(e.save() == SaveResult::Saved);
    CHECK(e.changes() == 0);
    const auto c = p.codeplug->channel(2);
    REQUIRE(c.has_value());
    CHECK(std::string(c->name) == "Edited");
    CHECK(c->power == dmm::services::PowerLevel::Turbo);
    const auto other = p.codeplug->channel(0);
    REQUIRE(other.has_value());
    CHECK(std::string(other->name) == "RPT 2M");
}

TEST_CASE("settings are one record with named fields")
{
    Plug p;
    auto write = [&](std::uint32_t a, std::span<const std::uint8_t> d) { return p.write(a, d); };
    D878Uv2PlugEditor e(*p.codeplug, write);
    REQUIRE(e.open(Book::Settings, 0));
    CHECK(text(e, find(e, "Time zone")) == "UTC+03:00");
    CHECK(text(e, find(e, "TX timeout")) == "Off");
    REQUIRE(e.setValue(find(e, "TX timeout"), 4));
    CHECK(text(e, find(e, "TX timeout")) == "120 s");
    CHECK(text(e, find(e, "Menu exit")) == "5 s");
    CHECK(e.fields() == find(e, "Notices") + 1 + 0x100);
}
