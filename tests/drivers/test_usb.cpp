// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tommi Tauriainen

#include "dmm/drivers/usb/cdc_acm.h"
#include "dmm/drivers/usb/descriptors.h"
#include "dmm/drivers/usb/device.h"

#include <doctest/doctest.h>

#include <cstring>
#include <vector>

using namespace dmm;
using namespace dmm::drivers::usb;

namespace {

// Records what the device stack asks of the controller and lets a test
// play the host.
class FakeController final : public hal::UsbDeviceController
{
public:
    struct Packet
    {
        std::uint8_t address;
        std::vector<std::uint8_t> data;
    };

    void start(hal::UsbDeviceEvents &e) override { events = &e; }
    void setAddress(std::uint8_t a) override { address = a; }
    void closeEndpoints() override { opened.clear(); }
    void openEndpoint(std::uint8_t a, hal::UsbEndpointType, std::uint16_t) override { opened.push_back(a); }
    std::size_t read(std::uint8_t, std::uint8_t *data, std::size_t capacity) override
    {
        const std::size_t n = std::min(capacity, outPacket.size());
        std::memcpy(data, outPacket.data(), n);
        return n;
    }
    void armOut(std::uint8_t a) override { armed.push_back(a); }
    void write(std::uint8_t a, const std::uint8_t *data, std::size_t length) override
    {
        sent.push_back({a, std::vector<std::uint8_t>(data, data + length)});
    }
    void stall(std::uint8_t a) override { stalled.push_back(a); }
    void lock() override { ++locks; }
    void unlock() override { --locks; }

    // Host side.
    void setup(std::uint8_t type, std::uint8_t request, std::uint16_t value, std::uint16_t index, std::uint16_t length)
    {
        const std::uint8_t raw[8] = {type, request, static_cast<std::uint8_t>(value), static_cast<std::uint8_t>(value >> 8),
            static_cast<std::uint8_t>(index), static_cast<std::uint8_t>(index >> 8), static_cast<std::uint8_t>(length),
            static_cast<std::uint8_t>(length >> 8)};
        events->onSetup(raw);
    }

    // Collects an IN control transfer: acknowledges each packet until the
    // device stops sending, then sends the status stage.
    std::vector<std::uint8_t> controlIn(std::uint8_t type, std::uint8_t request, std::uint16_t value,
        std::uint16_t index, std::uint16_t length)
    {
        sent.clear();
        setup(type, request, value, index, length);
        std::vector<std::uint8_t> data;
        std::size_t handled = 0;
        while (handled < sent.size()) {
            const Packet p = sent[handled++];
            REQUIRE(p.address == 0x80);
            data.insert(data.end(), p.data.begin(), p.data.end());
            events->onInComplete(0x80);
        }
        outPacket.clear();
        events->onOut(0x00);
        return data;
    }

    // An OUT control transfer with an optional data stage; true when the
    // device answered with an empty status packet.
    bool controlOut(std::uint8_t type, std::uint8_t request, std::uint16_t value, std::uint16_t index,
        std::vector<std::uint8_t> data = {})
    {
        sent.clear();
        stalled.clear();
        setup(type, request, value, index, static_cast<std::uint16_t>(data.size()));
        if (!data.empty()) {
            outPacket = data;
            events->onOut(0x00);
        }
        if (!stalled.empty() || sent.size() != 1 || sent[0].address != 0x80 || !sent[0].data.empty()) {
            return false;
        }
        events->onInComplete(0x80);
        return true;
    }

    hal::UsbDeviceEvents *events = nullptr;
    std::uint8_t address = 0;
    std::vector<std::uint8_t> opened;
    std::vector<std::uint8_t> armed;
    std::vector<Packet> sent;
    std::vector<std::uint8_t> stalled;
    std::vector<std::uint8_t> outPacket;
    int locks = 0;
};

constexpr auto DeviceDesc = compositeDeviceDescriptor({0x1209, 0x0001, 0x0100, 1, 2, 0});
constexpr auto Port = cdcAcmDescriptors(0, 0x81, 0x02, 0x82, 0);
constexpr auto ConfigDesc = join(configurationHeader(9 + CdcAcmDescriptorBytes, 2, 50), Port);
constexpr auto Lang = languageDescriptor();
constexpr auto Maker = stringDescriptor("DeltaMM");
constexpr auto Product = stringDescriptor("DeltaMM radio");
constexpr auto Exact64 = stringDescriptor("0123456789012345678901234567890"); // 64 bytes
const std::span<const std::uint8_t> Strings[] = {Lang, Maker, Product, Exact64};

struct Fixture
{
    FakeController controller;
    CdcAcm port{0, 0x81, 0x02, 0x82};
    Function *functions[1] = {&port};
    Device device{controller, Descriptors{DeviceDesc, ConfigDesc, Strings}, functions};

    Fixture()
    {
        device.start();
        controller.events->onBusReset();
    }

    void enumerate()
    {
        controller.controlIn(0x80, 6, 0x0100, 0, 64);
        REQUIRE(controller.controlOut(0x00, 5, 7, 0));
        controller.controlIn(0x80, 6, 0x0200, 0, 255);
        REQUIRE(controller.controlOut(0x00, 9, 1, 0));
    }

    void openPort() { REQUIRE(controller.controlOut(0x21, 0x22, 1, 0)); }
};

} // namespace

TEST_CASE("descriptors have the lengths their headers state")
{
    CHECK(DeviceDesc[0] == 18);
    CHECK(ConfigDesc.size() == 75);
    CHECK((ConfigDesc[2] | ConfigDesc[3] << 8) == 75);
    std::size_t at = 0;
    while (at < ConfigDesc.size()) {
        REQUIRE(ConfigDesc[at] > 0);
        at += ConfigDesc[at];
    }
    CHECK(at == ConfigDesc.size());
    CHECK(Maker.size() == 16);
    CHECK(Maker[0] == 16);
    CHECK(Maker[2] == 'D');
    CHECK(Maker[3] == 0);
}

TEST_CASE("enumeration: descriptors, address after the status stage, configuration")
{
    Fixture f;
    const auto device = f.controller.controlIn(0x80, 6, 0x0100, 0, 64);
    CHECK(device == std::vector<std::uint8_t>(DeviceDesc.begin(), DeviceDesc.end()));

    f.controller.sent.clear();
    f.controller.setup(0x00, 5, 7, 0, 0);
    CHECK(f.controller.address == 0);
    f.controller.events->onInComplete(0x80);
    CHECK(f.controller.address == 7);

    const auto shortConfig = f.controller.controlIn(0x80, 6, 0x0200, 0, 9);
    CHECK(shortConfig.size() == 9);
    const auto config = f.controller.controlIn(0x80, 6, 0x0200, 0, 255);
    CHECK(config == std::vector<std::uint8_t>(ConfigDesc.begin(), ConfigDesc.end()));
    const auto product = f.controller.controlIn(0x80, 6, 0x0302, 0x0409, 255);
    CHECK(product.size() == Product.size());

    CHECK_FALSE(f.device.configured());
    CHECK(f.controller.controlOut(0x00, 9, 1, 0));
    CHECK(f.device.configured());
    CHECK(f.controller.opened == std::vector<std::uint8_t>{0x81, 0x02, 0x82});
    CHECK(f.controller.controlIn(0x80, 8, 0, 0, 1) == std::vector<std::uint8_t>{1});
}

TEST_CASE("long replies go in 64-byte packets; a whole-packet reply shorter than asked ends with an empty one")
{
    Fixture f;
    const auto sizesFor = [&](std::uint16_t value, std::uint16_t length) {
        f.controller.sent.clear();
        f.controller.setup(0x80, 6, value, 0, length);
        std::vector<std::size_t> sizes;
        for (std::size_t i = 0; i < f.controller.sent.size(); ++i) {
            sizes.push_back(f.controller.sent[i].data.size());
            f.controller.events->onInComplete(0x80);
        }
        return sizes;
    };
    CHECK(sizesFor(0x0200, 255) == std::vector<std::size_t>{64, 11});
    CHECK(sizesFor(0x0303, 255) == std::vector<std::size_t>{64, 0});
    CHECK(sizesFor(0x0303, 64) == std::vector<std::size_t>{64});
}

TEST_CASE("unknown requests and descriptors are stalled")
{
    Fixture f;
    f.controller.setup(0x80, 6, 0x0600, 0, 10); // device qualifier
    CHECK(f.controller.stalled == std::vector<std::uint8_t>{0x80});
    CHECK_FALSE(f.controller.controlOut(0x00, 9, 2, 0));
    CHECK_FALSE(f.controller.controlOut(0x21, 0x7F, 0, 0));
}

TEST_CASE("serial port: line coding, DTR opens it, data goes out in packets")
{
    Fixture f;
    f.enumerate();
    CHECK_FALSE(f.port.isOpen());

    CHECK(f.controller.controlOut(0x21, 0x20, 0, 0, {0x80, 0x25, 0, 0, 0, 0, 8}));
    CHECK(f.controller.controlIn(0xA1, 0x21, 0, 0, 7) == std::vector<std::uint8_t>{0x80, 0x25, 0, 0, 0, 0, 8});

    // Closed: bytes are dropped but reported as written.
    const std::uint8_t hello[] = {'h', 'i'};
    f.controller.sent.clear();
    CHECK(f.port.write(hello) == 2);
    CHECK(f.controller.sent.empty());

    f.openPort();
    CHECK(f.port.isOpen());
    f.controller.sent.clear();
    CHECK(f.port.write(hello) == 2);
    REQUIRE(f.controller.sent.size() == 1);
    CHECK(f.controller.sent[0].address == 0x82);
    CHECK(f.controller.sent[0].data == std::vector<std::uint8_t>{'h', 'i'});
    CHECK(f.controller.locks == 0);

    // While a packet is in flight, more data waits, then follows in one packet.
    std::vector<std::uint8_t> block(100, 'x');
    CHECK(f.port.write(block) == 100);
    CHECK(f.controller.sent.size() == 1);
    f.controller.events->onInComplete(0x82);
    REQUIRE(f.controller.sent.size() == 2);
    CHECK(f.controller.sent[1].data.size() == 64);
    f.controller.events->onInComplete(0x82);
    REQUIRE(f.controller.sent.size() == 3);
    CHECK(f.controller.sent[2].data.size() == 36);
    f.controller.events->onInComplete(0x82);
    CHECK(f.controller.sent.size() == 3);
}

TEST_CASE("serial port: data ending on a full packet is followed by an empty one")
{
    Fixture f;
    f.enumerate();
    f.openPort();
    f.controller.sent.clear();
    std::vector<std::uint8_t> block(64, 'y');
    f.port.write(block);
    f.controller.events->onInComplete(0x82);
    REQUIRE(f.controller.sent.size() == 2);
    CHECK(f.controller.sent[1].data.empty());
    f.controller.events->onInComplete(0x82);
    CHECK(f.controller.sent.size() == 2);
}

TEST_CASE("serial port: received data is buffered, and held back when the buffer is full")
{
    Fixture f;
    f.enumerate();
    f.openPort();
    f.controller.armed.clear();
    f.controller.outPacket = {'a', 'b', 'c'};
    f.controller.events->onOut(0x02);
    CHECK(f.controller.armed == std::vector<std::uint8_t>{0x02});
    std::uint8_t buffer[8];
    CHECK(f.port.read(buffer) == 3);
    CHECK(buffer[2] == 'c');

    f.controller.outPacket.assign(64, 'z');
    f.controller.armed.clear();
    for (int i = 0; i < 5; ++i) {
        f.controller.events->onOut(0x02);
    }
    CHECK(f.controller.armed.size() == 4); // the fifth packet waits
    std::uint8_t big[64];
    CHECK(f.port.read(big) == 64);
    CHECK(f.controller.armed.size() == 5);
}

TEST_CASE("bus reset closes the port")
{
    Fixture f;
    f.enumerate();
    f.openPort();
    f.controller.events->onBusReset();
    CHECK_FALSE(f.port.isOpen());
    CHECK_FALSE(f.device.configured());
}
