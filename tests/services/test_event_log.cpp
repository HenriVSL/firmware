// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Henri Lavikainen

#include "dmm/services/event_log.h"

#include <doctest/doctest.h>

using dmm::services::EventLog;
using Kind = EventLog::Kind;

TEST_CASE("the event log keeps entries newest first, 12 bytes each")
{
    CHECK(sizeof(EventLog::Entry) == 12);
    EventLog log;
    CHECK(log.count() == 0);
    CHECK(log.newest(0) == nullptr);
    log.add(Kind::CallIn, 100, 2440001, 244, 12, true);
    log.add(Kind::AprsBeacon, 110);
    REQUIRE(log.count() == 2);
    CHECK(log.newest(0)->kind() == Kind::AprsBeacon);
    const EventLog::Entry *call = log.newest(1);
    REQUIRE(call != nullptr);
    CHECK(call->kind() == Kind::CallIn);
    CHECK(call->seconds() == 100);
    CHECK(call->a() == 2440001);
    CHECK(call->b() == 244);
    CHECK(call->value() == 12);
    CHECK(call->group());
    CHECK(log.newest(2) == nullptr);
}

TEST_CASE("a full event log drops its oldest entry")
{
    EventLog log;
    for (std::uint32_t i = 0; i < EventLog::Capacity + 3; ++i) {
        log.add(Kind::RoamMove, i, i + 1);
    }
    CHECK(log.count() == EventLog::Capacity);
    CHECK(log.newest(0)->a() == EventLog::Capacity + 3);
    CHECK(log.newest(EventLog::Capacity - 1)->a() == 4);
}

TEST_CASE("lists of some kinds share the one buffer")
{
    EventLog log;
    log.add(Kind::CallIn, 1, 7, 9);
    log.add(Kind::AprsBeacon, 2);
    log.add(Kind::CallOut, 3, 0, 9);
    log.add(Kind::AprsBeacon, 4);
    const auto aprs = EventLog::only(Kind::AprsBeacon);
    CHECK(log.count(aprs) == 2);
    CHECK(log.newest(0, aprs)->seconds() == 4);
    CHECK(log.newest(1, aprs)->seconds() == 2);
    CHECK(log.newest(2, aprs) == nullptr);
    const auto calls = EventLog::only(Kind::CallIn) | EventLog::only(Kind::CallOut);
    CHECK(log.count(calls) == 2);
    CHECK(log.newest(0, calls)->kind() == Kind::CallOut);
}

TEST_CASE("a call still going on updates its length; long values are held at 127")
{
    EventLog log;
    log.add(Kind::CallIn, 1, 7, 9, 0, true);
    log.add(Kind::AprsBeacon, 2);
    log.update(Kind::CallIn, 5);
    CHECK(log.newest(1)->value() == 5);
    CHECK(log.newest(1)->group());
    CHECK(log.newest(1)->b() == 9);
    log.update(Kind::CallIn, 400);
    CHECK(log.newest(1)->value() == 127);
    log.update(Kind::GpsFix, 3); // none of that kind: nothing changes
    CHECK(log.newest(0)->value() == 0);
}
