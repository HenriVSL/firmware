// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tommi Tauriainen

#include "dmm/drivers/radio/rda1846.h"

#include <doctest/doctest.h>

#include <map>
#include <vector>

using dmm::drivers::radio::Rda1846;

namespace {

class FakeBus final : public dmm::hal::RegisterBus
{
public:
    std::uint16_t read(std::uint8_t address) override { return registers[address]; }
    void write(std::uint8_t address, std::uint16_t value) override
    {
        registers[address] = value;
        writes.push_back(address);
    }

    std::map<std::uint8_t, std::uint16_t> registers;
    std::vector<std::uint8_t> writes;
};

class LoggingBus final : public dmm::hal::RegisterBus
{
public:
    std::uint16_t read(std::uint8_t address) override { return registers[address]; }
    void write(std::uint8_t address, std::uint16_t value) override
    {
        registers[address] = value;
        log.emplace_back(address, value);
    }

    std::map<std::uint8_t, std::uint16_t> registers;
    std::vector<std::pair<std::uint8_t, std::uint16_t>> log;
};

unsigned sleptMs = 0;
void sleep(unsigned ms)
{
    sleptMs += ms;
}

using Write = std::pair<std::uint8_t, std::uint16_t>;

} // namespace

TEST_CASE("the RDA1846 is found by its chip ID")
{
    FakeBus bus;
    Rda1846 chip(bus);
    CHECK_FALSE(chip.present());
    bus.registers[0x00] = 0x1846;
    CHECK(chip.present());
    CHECK(bus.writes.empty());
}

TEST_CASE("setGpio changes one pin's two bits and keeps the rest")
{
    FakeBus bus;
    Rda1846 chip(bus);
    bus.registers[0x1F] = 0x5AA2;
    CHECK(chip.setGpio(3, Rda1846::GpioMode::High) == 0x5AE2);
    CHECK(bus.registers[0x1F] == 0x5AE2);
    CHECK(chip.setGpio(0, Rda1846::GpioMode::HighImpedance) == 0x5AE0);
    CHECK(chip.setGpio(7, Rda1846::GpioMode::Low) == 0x9AE0);
    CHECK(bus.writes == std::vector<std::uint8_t>{0x1F, 0x1F, 0x1F});
    // An out-of-range pin writes nothing.
    chip.setGpio(8, Rda1846::GpioMode::High);
    CHECK(bus.writes.size() == 3);
}

TEST_CASE("frequency and squelch words follow the programming guide's examples")
{
    CHECK(Rda1846::frequencyWord(409'750'000) == 0x640960);
    CHECK(Rda1846::frequencyWord(145'500'000) == 145'500u * 16);
    CHECK(Rda1846::squelchWord(-120, -122) == 0x088F);
    // Levels past the field's range are clamped.
    CHECK(Rda1846::squelchWord(-140, 0) == 0x007F);
}

TEST_CASE("start resets, restores the GPIO pins before calibrating, and leaves the chip idle")
{
    LoggingBus bus;
    bus.registers[0x00] = 0x1846;
    Rda1846 chip(bus);
    sleptMs = 0;
    REQUIRE(chip.start(sleep, 0x00C0));
    REQUIRE(bus.log.size() > 4);
    CHECK(bus.log[0] == Write{0x30, 0x0001});
    CHECK(bus.log[1] == Write{0x30, 0x0004});
    CHECK(bus.log[2] == Write{0x1F, 0x00C0});
    CHECK(bus.registers[0x30] == 0x4006);
    CHECK(bus.registers[0x04] == 0x0FD0);
    CHECK(bus.registers[0x7F] == 0);
    CHECK(sleptMs == 210);
    CHECK(chip.mode() == Rda1846::Mode::Idle);

    LoggingBus none;
    Rda1846 missing(none);
    CHECK_FALSE(missing.start(sleep, 0));
    CHECK(none.log.empty());
}

TEST_CASE("receive and transmit always pass through idle")
{
    LoggingBus bus;
    Rda1846 chip(bus);
    chip.setMode(Rda1846::Mode::Receive);
    CHECK(bus.log ==
        std::vector<Write>{{0x30, 0x4006}, {0x0A, 0x4380}, {0x44, 0x09F9}, {0x58, 0x8085}, {0x30, 0x4026}});
    bus.log.clear();
    chip.setBandwidth(Rda1846::Bandwidth::Wide);
    bus.log.clear();
    chip.setMode(Rda1846::Mode::Transmit);
    CHECK(bus.log == std::vector<Write>{{0x30, 0x7006}, {0x0A, 0x3BA0}, {0x41, 0x070F}, {0x44, 0x09FF}, {0x58, 0x8405}, {0x30, 0x7046}});
    bus.log.clear();
    chip.setMode(Rda1846::Mode::Idle);
    CHECK(bus.log == std::vector<Write>{{0x30, 0x7006}});
}

TEST_CASE("tuning picks the band setting near the reference harmonics")
{
    LoggingBus bus;
    Rda1846 chip(bus);
    chip.setFrequency(433'500'000);
    CHECK(bus.registers[0x05] == 0x8763);
    CHECK(bus.registers[0x29] == (Rda1846::frequencyWord(433'500'000) >> 16));
    CHECK(bus.registers[0x2A] == (Rda1846::frequencyWord(433'500'000) & 0xFFFF));
    chip.setFrequency(455'025'000);
    CHECK(bus.registers[0x05] == 0x8543);
    chip.setFrequency(145'500'000);
    CHECK(bus.registers[0x05] == 0x8763);
}

TEST_CASE("RSSI, transmit tone and deviation")
{
    LoggingBus bus;
    Rda1846 chip(bus);
    bus.registers[0x1B] = 0x2A40; // 42 dB over the floor, noise 64 dB
    CHECK(chip.rssiDbm() == -95);
    CHECK(chip.noiseDb() == 64);
    chip.setNoiseSquelch(56, 53);
    CHECK(bus.registers[0x48] == ((56 << 7) | 53));
    chip.setTransmitTone(11'880);
    CHECK(bus.registers[0x4A] == 11'880);
    CHECK(bus.registers[0x4E] == 0x2602);
    chip.setTransmitTone(0);
    CHECK(bus.registers[0x4E] == 0x2002);
    chip.setDeviation(0x50, 0x12);
    CHECK(bus.registers[0x59] == ((0x50 << 6) | 0x12));
}
