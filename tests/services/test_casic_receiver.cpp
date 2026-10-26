// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Henri Lavikainen

#include "dmm/services/casic_receiver.h"

#include <doctest/doctest.h>

#include <string>
#include <vector>

using namespace dmm::services;

namespace {

std::string withChecksum(const std::string &body)
{
    unsigned sum = 0;
    for (char c : body) {
        sum ^= static_cast<unsigned char>(c);
    }
    const char *hex = "0123456789ABCDEF";
    return "$" + body + "*" + hex[sum >> 4] + hex[sum & 15] + "\r\n";
}

struct Recorder
{
    std::vector<std::string> sent;
    void operator()(std::span<const std::uint8_t> bytes)
    {
        sent.emplace_back(reinterpret_cast<const char *>(bytes.data()), bytes.size());
    }
};

} // namespace

TEST_CASE("casic sentences carry their checksum")
{
    CHECK(std::string(CasicReceiver::sentence("PCAS04,3").view()) == withChecksum("PCAS04,3"));
    CHECK(std::string(CasicReceiver::sentence("PCAS06,0").view()) == withChecksum("PCAS06,0"));
}

TEST_CASE("casic configuration asks for RMC, GGA and GSV and the chosen systems")
{
    CasicReceiver receiver;
    Recorder out;
    receiver.configure(gps_systems::Gps | gps_systems::Glonass, out);
    REQUIRE(out.sent.size() == 2);
    CHECK(out.sent[0] == withChecksum("PCAS03,1,0,0,1,1,0,0,0"));
    CHECK(out.sent[1] == withChecksum("PCAS04,5"));

    out.sent.clear();
    receiver.configure(0, out);
    CHECK(out.sent.back() == withChecksum("PCAS04,3"));
    out.sent.clear();
    receiver.configure(gps_systems::All, out);
    CHECK(out.sent.back() == withChecksum("PCAS04,7"));
}

TEST_CASE("casic console commands")
{
    CasicReceiver receiver;
    Recorder out;
    CHECK(receiver.debugCommand("query", out));
    CHECK(receiver.debugCommand("restart cold", out));
    CHECK(receiver.debugCommand("restart hot", out));
    CHECK(receiver.debugCommand("send $PCAS06,1", out));
    CHECK(receiver.debugCommand("send PCAS02,1000", out));
    REQUIRE(out.sent.size() == 5);
    CHECK(out.sent[0] == withChecksum("PCAS06,0"));
    CHECK(out.sent[1] == withChecksum("PCAS10,2"));
    CHECK(out.sent[2] == withChecksum("PCAS10,0"));
    CHECK(out.sent[3] == withChecksum("PCAS06,1"));
    CHECK(out.sent[4] == withChecksum("PCAS02,1000"));

    CHECK_FALSE(receiver.debugCommand("send ", out));
    CHECK_FALSE(receiver.debugCommand("restart later", out));
    CHECK_FALSE(receiver.debugCommand("nonsense", out));
    CHECK(out.sent.size() == 5);
}
