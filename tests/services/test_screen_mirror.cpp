// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Henri Lavikainen

#include "dmm/services/screen_mirror.h"

#include <doctest/doctest.h>

#include <cstdint>

using dmm::services::ScreenMirror;
namespace colors = dmm::services::colors;

TEST_CASE("the screen mirror keeps what was drawn, by colour index")
{
    ScreenMirror<16, 8> m;
    CHECK(m.at(0, 0) == 0); // black at start
    m.fill(2, 1, 3, 2, colors::Red);
    CHECK(m.colourCount() == 2);
    CHECK(m.colours()[1] == colors::Red);
    CHECK(m.at(2, 1) == 1);
    CHECK(m.at(4, 2) == 1);
    CHECK(m.at(5, 2) == 0);
    m.fill(-5, -5, 100, 1, colors::White); // clipped to nothing
    CHECK(m.colourCount() == 3);
    CHECK(m.at(0, 0) == 0);
    const std::uint8_t rows[] = {0b10100000, 0b01000000};
    m.bitmap(8, 0, 3, 2, rows, 1, colors::Green, colors::Black);
    CHECK(m.at(8, 0) == 3);
    CHECK(m.at(9, 0) == 0);
    CHECK(m.at(10, 0) == 3);
    CHECK(m.at(9, 1) == 3);
}

TEST_CASE("the screen mirror counts pixels changed twice in one frame")
{
    ScreenMirror<16, 8> m;
    m.fill(0, 0, 4, 1, colors::White);
    m.endFrame();
    m.fill(0, 0, 4, 1, colors::Black); // cleared...
    m.fill(0, 0, 4, 1, colors::White); // ...and drawn again: flicker
    m.fill(4, 0, 2, 1, colors::White); // drawn once: none
    m.endFrame();
    std::uint32_t worst = 0;
    CHECK(m.takeFlicker(worst) == 4);
    CHECK(worst == 4);
    CHECK(m.takeFlicker(worst) == 0);
}

TEST_CASE("past sixteen colours the nearest one is taken")
{
    ScreenMirror<16, 8> m;
    for (unsigned i = 1; i < 16; ++i) {
        m.fill(static_cast<int>(i), 0, 1, 1, dmm::services::rgb(i * 16, 0, 0));
    }
    m.fill(0, 1, 1, 1, dmm::services::rgb(15 * 16 + 4, 0, 0));
    CHECK(m.colourCount() == 16);
    CHECK(m.at(0, 1) == 15);
}
