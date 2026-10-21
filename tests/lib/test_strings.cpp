// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tommi Tauriainen

#include "dmm/lib/strings.h"

#include <doctest/doctest.h>

using dmm::lib::endsWith;
using dmm::lib::startsWith;

TEST_CASE("startsWith and endsWith match whole prefixes and suffixes")
{
    CHECK(startsWith("autooff 5", "autooff "));
    CHECK(startsWith("abc", ""));
    CHECK(startsWith("", ""));
    CHECK_FALSE(startsWith("auto", "autooff "));
    CHECK_FALSE(startsWith("xautooff", "autooff"));

    CHECK(endsWith("pin pa7 1", " 1"));
    CHECK(endsWith("abc", ""));
    CHECK_FALSE(endsWith("1", " 1"));
    CHECK_FALSE(endsWith("pin pa7 0", " 1"));
}
