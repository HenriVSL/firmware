// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tommi Tauriainen

#include "dmm/lib/fixed_vector.h"
#include "dmm/lib/fn_ref.h"
#include "dmm/lib/ring_buffer.h"
#include <doctest/doctest.h>

using namespace dmm::lib;

namespace {

int twice(int x)
{
    return 2 * x;
}

int apply(FnRef<int(int)> f, int x)
{
    return f(x);
}

struct Counted
{
    static inline int alive = 0;
    int v;
    explicit Counted(int value)
        : v(value)
    {
        ++alive;
    }
    Counted(const Counted &o)
        : v(o.v)
    {
        ++alive;
    }
    Counted &operator=(const Counted &) = default;
    ~Counted() { --alive; }
};

} // namespace

TEST_CASE("FnRef calls lambdas, functors and free functions")
{
    int offset = 5;
    auto add = [&](int x) { return x + offset; };
    CHECK(apply(add, 1) == 6);
    offset = 10;
    CHECK(apply(add, 1) == 11);
    CHECK(apply(twice, 4) == 8);
    CHECK(apply(&twice, 5) == 10);
}

TEST_CASE("RingBuffer keeps FIFO order and refuses when full")
{
    RingBuffer<int, 3> rb;
    CHECK(rb.empty());
    CHECK(rb.push(1));
    CHECK(rb.push(2));
    CHECK(rb.push(3));
    CHECK(rb.full());
    CHECK_FALSE(rb.push(4));
    CHECK(rb.pop() == 1);
    CHECK(rb.push(4));
    CHECK(rb.pop() == 2);
    CHECK(rb.pop() == 3);
    CHECK(rb.pop() == 4);
    CHECK_FALSE(rb.pop().has_value());
}

TEST_CASE("FixedVector constructs and destroys in place")
{
    {
        FixedVector<Counted, 2> v;
        CHECK(v.emplaceBack(1));
        CHECK(v.pushBack(Counted{2}));
        CHECK_FALSE(v.emplaceBack(3));
        CHECK(v.size() == 2);
        CHECK(v[1].v == 2);
        CHECK(Counted::alive == 2);
        FixedVector<Counted, 2> copy = v;
        CHECK(Counted::alive == 4);
        v.popBack();
        CHECK(Counted::alive == 3);
    }
    CHECK(Counted::alive == 0);
}
