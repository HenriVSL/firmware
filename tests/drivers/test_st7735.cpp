// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tommi Tauriainen

#include "dmm/drivers/display/st7735.h"

#include <doctest/doctest.h>

#include <vector>

using namespace dmm;
using dmm::drivers::display::St7735;

namespace {

// Keeps a picture of the controller's memory, as the controller would:
// commands set the window, pixels fill it in address order.
class FakeLcd final : public hal::LcdBus
{
public:
    void command(std::uint8_t code) override
    {
        commands.push_back(code);
        m_current = code;
        m_params.clear();
        if (code == 0x2C) {
            m_col = m_c0;
            m_row = m_r0;
            m_highByte = true;
        }
    }
    void data(const std::uint8_t *bytes, std::size_t count) override
    {
        for (std::size_t i = 0; i < count; ++i) {
            dataByte(bytes[i]);
        }
    }
    void repeat(std::uint16_t pixel, std::uint32_t count) override
    {
        ++repeatCalls;
        for (std::uint32_t i = 0; i < count; ++i) {
            dataByte(static_cast<std::uint8_t>(pixel >> 8));
            dataByte(static_cast<std::uint8_t>(pixel));
        }
    }
    void release() override { ++releases; }
    void setReset(bool active) override { resets.push_back(active); }

    std::uint16_t at(int column, int row) const { return gram[row * 132 + column]; }

    std::vector<std::uint8_t> commands;
    std::vector<bool> resets;
    std::vector<std::uint8_t> madctl;
    int releases = 0;
    int repeatCalls = 0;
    std::vector<std::uint16_t> gram = std::vector<std::uint16_t>(132 * 162, 0xDEAD);

private:
    void dataByte(std::uint8_t b)
    {
        if (m_current == 0x2C) {
            if (m_highByte) {
                m_pending = b;
            } else {
                gram[m_row * 132 + m_col] = static_cast<std::uint16_t>(m_pending << 8 | b);
                if (++m_col > m_c1) {
                    m_col = m_c0;
                    ++m_row;
                }
            }
            m_highByte = !m_highByte;
            return;
        }
        m_params.push_back(b);
        if (m_current == 0x2A && m_params.size() == 4) {
            m_c0 = m_params[0] << 8 | m_params[1];
            m_c1 = m_params[2] << 8 | m_params[3];
        } else if (m_current == 0x2B && m_params.size() == 4) {
            m_r0 = m_params[0] << 8 | m_params[1];
            m_r1 = m_params[2] << 8 | m_params[3];
        } else if (m_current == 0x36) {
            madctl.push_back(b);
        }
    }

    std::uint8_t m_current = 0;
    std::vector<std::uint8_t> m_params;
    int m_c0 = 0, m_c1 = 0, m_r0 = 0, m_r1 = 0, m_col = 0, m_row = 0;
    bool m_highByte = true;
    std::uint8_t m_pending = 0;
};

void noSleep(unsigned) {}

} // namespace

TEST_CASE("init resets, wakes, sets RGB565 and the access byte, clears and turns on")
{
    FakeLcd lcd;
    St7735 panel(lcd, {160, 128, true, 0x88});
    panel.init(noSleep);
    CHECK(lcd.resets == std::vector<bool>{true, false});
    CHECK(lcd.commands == std::vector<std::uint8_t>{0x01, 0x11, 0x3A, 0x36, 0x2A, 0x2B, 0x2C, 0x29});
    CHECK(lcd.madctl == std::vector<std::uint8_t>{0x88});
    CHECK(lcd.at(0, 0) == 0);
    CHECK(lcd.at(127, 159) == 0);
    CHECK(lcd.at(128, 0) == 0xDEAD); // outside the 128 x 160 area
}

TEST_CASE("with swapped axes, screen x runs along controller rows")
{
    FakeLcd lcd;
    St7735 panel(lcd, {160, 128, true, 0x88});
    panel.fillRect(10, 20, 3, 2, 0x1234); // x 10..12, y 20..21
    CHECK(lcd.at(20, 10) == 0x1234);
    CHECK(lcd.at(21, 12) == 0x1234);
    CHECK(lcd.at(22, 10) == 0xDEAD);
    CHECK(lcd.at(20, 13) == 0xDEAD);
}

TEST_CASE("bitmaps land where they belong, in runs, and are clipped")
{
    FakeLcd lcd;
    St7735 panel(lcd, {160, 128, true, 0x88});
    // 3 x 2: top row 101, bottom row 010.
    const std::uint8_t rows[] = {0b10100000, 0b01000000};
    panel.drawBitmap(158, 5, 3, 2, rows, 1, 0xFFFF, 0x0000);
    CHECK(lcd.at(5, 158) == 0xFFFF); // (0,0) set
    CHECK(lcd.at(6, 158) == 0x0000); // (0,1) clear
    CHECK(lcd.at(5, 159) == 0x0000); // (1,0) clear
    CHECK(lcd.at(6, 159) == 0xFFFF); // (1,1) set
    CHECK(lcd.at(5, 160) == 0xDEAD); // column 2 clipped off the screen
    CHECK(lcd.repeatCalls == 3);     // set, clear clear, set

    St7735 upright(lcd, {128, 160, false, 0x00});
    upright.drawBitmap(0, 0, 3, 1, rows, 1, 0x00F0, 0x000F);
    CHECK(lcd.at(0, 0) == 0x00F0);
    CHECK(lcd.at(1, 0) == 0x000F);
    CHECK(lcd.at(2, 0) == 0x00F0);
}

TEST_CASE("nothing is sent for a rectangle fully off the screen")
{
    FakeLcd lcd;
    St7735 panel(lcd, {160, 128, true, 0x88});
    panel.fillRect(160, 0, 5, 5, 1);
    panel.fillRect(0, -10, 5, 10, 1);
    panel.fillRect(0, 0, 0, 5, 1);
    CHECK(lcd.commands.empty());
}
