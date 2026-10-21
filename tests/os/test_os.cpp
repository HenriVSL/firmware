// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tommi Tauriainen

#include "dmm/os/os.h"

#include <doctest/doctest.h>

#include <atomic>

using namespace dmm;

namespace {

struct Ping
{
    int value;
};

os::Queue<Ping, 4> requests;
os::Queue<Ping, 4> replies;
os::Task<256> echoTask;

void echo(void *)
{
    Ping p{};
    while (requests.receive(p, 100) || !os::stopRequested()) {
        if (p.value != 0) {
            replies.send(Ping{p.value * 2}, os::WaitForever);
            p.value = 0;
        }
    }
}

os::Mutex counterLock;
int counter = 0;
std::atomic<int> finished{0};
os::Task<256> adderA;
os::Task<256> adderB;

void add(void *)
{
    for (int i = 0; i < 10000; ++i) {
        counterLock.lock();
        ++counter;
        counterLock.unlock();
    }
    ++finished;
}

} // namespace

TEST_CASE("tasks exchange messages through queues")
{
    echoTask.start("echo", 3, echo, nullptr);
    os::runScheduler();
    for (int i = 1; i <= 10; ++i) {
        REQUIRE(requests.send(Ping{i}, 1000));
        Ping reply{};
        REQUIRE(replies.receive(reply, 1000));
        CHECK(reply.value == 2 * i);
    }
    Ping none{};
    CHECK_FALSE(replies.receive(none, 20));

    adderA.start("adderA", 2, add, nullptr);
    adderB.start("adderB", 2, add, nullptr);
    while (finished.load() < 2) {
        os::sleepMs(1);
    }
    CHECK(counter == 20000);

    os::requestStop();
    os::joinAll();
}

TEST_CASE("sleepUntil keeps a fixed period")
{
    os::Ticks wake = os::nowMs();
    const os::Ticks start = wake;
    for (int i = 0; i < 5; ++i) {
        os::sleepUntil(wake, 20);
    }
    const os::Ticks elapsed = os::nowMs() - start;
    CHECK(elapsed >= 100);
    CHECK(elapsed < 160);
    CHECK(wake == start + 100);
}

TEST_CASE("a full queue refuses after its timeout")
{
    os::Queue<Ping, 2> q;
    CHECK(q.send(Ping{1}));
    CHECK(q.send(Ping{2}));
    CHECK_FALSE(q.send(Ping{3}, 10));
    Ping p{};
    CHECK(q.receive(p, 0));
    CHECK(p.value == 1);
}
