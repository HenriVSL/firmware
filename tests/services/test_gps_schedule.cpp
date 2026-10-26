// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Henri Lavikainen

#include "dmm/services/gps_schedule.h"

#include <doctest/doctest.h>

#include <utility>

using namespace dmm::services;
using Action = GpsSchedule::Action;
using Client = GpsSchedule::Client;

namespace {

constexpr std::uint32_t S = 1000;
constexpr std::uint32_t Min = 60 * S;

// Steps the schedule second by second from @p from to @p to, following
// its actions, and returns the first action other than None and its time.
struct Run
{
    GpsSchedule &schedule;
    bool powered = false;

    std::pair<Action, std::uint32_t> until(std::uint32_t from, std::uint32_t to)
    {
        for (std::uint32_t t = from; t <= to; t += S) {
            const Action a = schedule.update(t, powered);
            if (a != Action::None) {
                powered = a == Action::PowerOn;
                return {a, t};
            }
        }
        return {Action::None, to};
    }
};

} // namespace

TEST_CASE("gps schedule: without power saving the receiver stays on")
{
    GpsSchedule s;
    s.setNeed(Client::Clock, 10 * Min);
    s.setEnabled(true);
    Run run{s};
    CHECK(run.until(0, 0) == std::pair{Action::PowerOn, 0u});
    s.fix(40 * S);
    CHECK(run.until(41 * S, 60 * Min).first == Action::None);
    CHECK(run.powered);
}

TEST_CASE("gps schedule: the clock alone lets the receiver sleep between fixes")
{
    GpsSchedule s;
    s.setPowerSave(true);
    s.setNeed(Client::Clock, 10 * Min);
    s.setEnabled(true);
    Run run{s};
    CHECK(run.until(0, 0) == std::pair{Action::PowerOn, 0u});
    CHECK(run.until(S, 39 * S).first == Action::None);
    s.fix(40 * S);
    // It stays on a moment for the rest of the second's sentences.
    CHECK(run.until(40 * S, 60 * S) == std::pair{Action::PowerOff, 40 * S + GpsSchedule::SettleMs});
    CHECK(s.asleep(run.powered));
    // The start took 40 s, so the next one begins 50 s before the fix is due.
    CHECK(s.leadMs() == 50 * S);
    CHECK(run.until(41 * S, 10 * Min + 40 * S) == std::pair{Action::PowerOn, 10 * Min - 10 * S});
}

TEST_CASE("gps schedule: a start without a fix is given up and tried again later")
{
    GpsSchedule s;
    s.setPowerSave(true);
    s.setNeed(Client::Clock, 10 * Min);
    s.setEnabled(true);
    Run run{s};
    run.until(0, 0);
    CHECK(run.until(S, 20 * Min) == std::pair{Action::PowerOff, GpsSchedule::GiveUpMs});
    CHECK(run.until(GpsSchedule::GiveUpMs + S, 30 * Min) == std::pair{Action::PowerOn, GpsSchedule::GiveUpMs + 10 * Min});
}

TEST_CASE("gps schedule: the GPS screen keeps it on, closing it lets it sleep")
{
    GpsSchedule s;
    s.setPowerSave(true);
    s.setNeed(Client::Clock, 10 * Min);
    s.setEnabled(true);
    Run run{s};
    run.until(0, 0);
    s.fix(30 * S);
    run.until(30 * S, 60 * S); // off
    REQUIRE_FALSE(run.powered);
    s.setNeed(Client::Screen, S);
    CHECK(run.until(2 * Min, 2 * Min) == std::pair{Action::PowerOn, 2 * Min});
    s.fix(3 * Min);
    CHECK(run.until(3 * Min, 9 * Min).first == Action::None);
    s.setNeed(Client::Screen, 0);
    CHECK(run.until(9 * Min, 9 * Min) == std::pair{Action::PowerOff, 9 * Min});
}

TEST_CASE("gps schedule: a client that needs positions more often wakes it earlier")
{
    GpsSchedule s;
    s.setPowerSave(true);
    s.setNeed(Client::Clock, 30 * Min);
    s.setEnabled(true);
    Run run{s};
    run.until(0, 0);
    s.fix(40 * S);
    run.until(40 * S, 60 * S); // off; lead 50 s
    // Another client wants a position every 5 minutes: due 5 min after the
    // last fix.
    s.setNeed(Client::Console, 5 * Min);
    CHECK(run.until(41 * S, 30 * Min) == std::pair{Action::PowerOn, 5 * Min + 40 * S - 50 * S});
    // Every 2 minutes is short enough to stay on.
    s.fix(5 * Min + 20 * S);
    s.setNeed(Client::Console, 2 * Min);
    CHECK(run.until(5 * Min + 20 * S, 60 * Min).first == Action::None);
    CHECK(run.powered);
}

TEST_CASE("gps schedule: switching the GPS off powers it down")
{
    GpsSchedule s;
    s.setNeed(Client::Clock, 10 * Min);
    s.setEnabled(true);
    Run run{s};
    run.until(0, 0);
    s.setEnabled(false);
    CHECK(run.until(S, S) == std::pair{Action::PowerOff, S});
    CHECK(run.until(2 * S, 60 * Min).first == Action::None);
}
