// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Henri Lavikainen

#pragma once

namespace dmm::lib {

// Math for tables and settings worked out at compile time: plain series
// in double precision, accurate to about 1e-15 and slow at run time.

inline constexpr double Pi = 3.14159265358979323846;

/// cos(x), from its Taylor series around pi after reducing @p x to
/// 0 <= x < 2 pi.
constexpr double cosine(double x)
{
    while (x < 0.0) {
        x += 2.0 * Pi;
    }
    while (x >= 2.0 * Pi) {
        x -= 2.0 * Pi;
    }
    const double y = x - Pi;
    double term = 1.0;
    double sum = 1.0;
    for (int n = 1; n < 14; ++n) {
        term *= -y * y / ((2 * n - 1) * (2 * n));
        sum += term;
    }
    return -sum;
}

constexpr double sine(double x)
{
    return cosine(x - Pi / 2.0);
}

/// e to the power @p x: e to the nearest whole power times the Taylor
/// series of the rest, which is at most 1/2.
constexpr double exponential(double x)
{
    constexpr double E = 2.71828182845904523536;
    const int whole = static_cast<int>(x < 0.0 ? x - 0.5 : x + 0.5);
    const double rest = x - whole;
    double term = 1.0;
    double sum = 1.0;
    for (int n = 1; n < 20; ++n) {
        term *= rest / n;
        sum += term;
    }
    for (int i = 0; i < whole; ++i) {
        sum *= E;
    }
    for (int i = 0; i > whole; --i) {
        sum /= E;
    }
    return sum;
}

/// 10 to the power @p x.
constexpr double power10(double x)
{
    constexpr double Ln10 = 2.30258509299404568402;
    return exponential(x * Ln10);
}

} // namespace dmm::lib
