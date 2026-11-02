// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Henri Lavikainen

#include "dmm/lib/constexpr_math.h"

#include <doctest/doctest.h>

#include <cmath>

using namespace dmm::lib;

TEST_CASE("the compile-time cosine and sine follow the library's")
{
    for (double x = -10.0; x <= 10.0; x += 0.01) {
        CHECK(std::abs(cosine(x) - std::cos(x)) < 1e-12);
        CHECK(std::abs(sine(x) - std::sin(x)) < 1e-12);
    }
    static_assert(cosine(0.0) > 0.999999999999 && cosine(Pi) < -0.999999999999);
}

TEST_CASE("the compile-time powers follow the library's")
{
    for (double x = -6.0; x <= 6.0; x += 0.01) {
        CHECK(std::abs(exponential(x) / std::exp(x) - 1.0) < 1e-12);
        CHECK(std::abs(power10(x / 4.0) / std::pow(10.0, x / 4.0) - 1.0) < 1e-12);
    }
    static_assert(power10(1.0) > 9.999999999 && power10(1.0) < 10.000000001);
}
