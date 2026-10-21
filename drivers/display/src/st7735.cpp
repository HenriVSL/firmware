// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tommi Tauriainen

#include "dmm/drivers/display/st7735.h"

namespace dmm::drivers::display {

namespace {

namespace cmd {
constexpr std::uint8_t SoftwareReset = 0x01;
constexpr std::uint8_t SleepIn = 0x10;
constexpr std::uint8_t SleepOut = 0x11;
constexpr std::uint8_t DisplayOn = 0x29;
constexpr std::uint8_t ColumnAddress = 0x2A;
constexpr std::uint8_t RowAddress = 0x2B;
constexpr std::uint8_t MemoryWrite = 0x2C;
constexpr std::uint8_t MemoryAccess = 0x36;
constexpr std::uint8_t PixelFormat = 0x3A;
} // namespace cmd

constexpr std::uint8_t Rgb565 = 0x55;

// Datasheet waits: 120 ms after a reset or leaving sleep before the next
// sleep command, 5 ms after a software reset before any command.
constexpr unsigned ResetPulseMs = 10;
constexpr unsigned AfterResetMs = 120;
constexpr unsigned AfterSleepChangeMs = 120;

} // namespace

St7735::St7735(hal::LcdBus &bus, const Geometry &geometry)
    : m_bus(bus), m_geometry(geometry)
{
}

void St7735::sendCommand(std::uint8_t code, std::initializer_list<std::uint8_t> parameters)
{
    m_bus.command(code);
    m_bus.data(parameters.begin(), parameters.size());
    m_bus.release();
}

void St7735::init(lib::FnRef<void(unsigned)> sleepMs)
{
    m_bus.setReset(true);
    sleepMs(ResetPulseMs);
    m_bus.setReset(false);
    sleepMs(AfterResetMs);
    sendCommand(cmd::SoftwareReset);
    sleepMs(AfterResetMs);
    sendCommand(cmd::SleepOut);
    sleepMs(AfterSleepChangeMs);
    sendCommand(cmd::PixelFormat, {Rgb565});
    sendCommand(cmd::MemoryAccess, {m_geometry.memoryAccess});
    fillRect(0, 0, m_geometry.width, m_geometry.height, 0);
    sendCommand(cmd::DisplayOn);
}

void St7735::setSleep(bool asleep, lib::FnRef<void(unsigned)> sleepMs)
{
    sendCommand(asleep ? cmd::SleepIn : cmd::SleepOut);
    sleepMs(AfterSleepChangeMs);
}

bool St7735::clip(int x, int y, int w, int h, Rect &r) const
{
    r = {x < 0 ? 0 : x, y < 0 ? 0 : y, x + w - 1, y + h - 1};
    if (r.x1 >= m_geometry.width) {
        r.x1 = m_geometry.width - 1;
    }
    if (r.y1 >= m_geometry.height) {
        r.y1 = m_geometry.height - 1;
    }
    return w > 0 && h > 0 && r.x0 <= r.x1 && r.y0 <= r.y1;
}

// Opens the window and starts a memory write. Pixels then fill it row by
// row on the screen, or column by column when the axes are swapped.
void St7735::beginWrite(const Rect &r)
{
    const int c0 = m_geometry.axesSwapped ? r.y0 : r.x0;
    const int c1 = m_geometry.axesSwapped ? r.y1 : r.x1;
    const int r0 = m_geometry.axesSwapped ? r.x0 : r.y0;
    const int r1 = m_geometry.axesSwapped ? r.x1 : r.y1;
    m_bus.command(cmd::ColumnAddress);
    const std::uint8_t columns[] = {static_cast<std::uint8_t>(c0 >> 8), static_cast<std::uint8_t>(c0),
        static_cast<std::uint8_t>(c1 >> 8), static_cast<std::uint8_t>(c1)};
    m_bus.data(columns, sizeof columns);
    m_bus.command(cmd::RowAddress);
    const std::uint8_t rows[] = {static_cast<std::uint8_t>(r0 >> 8), static_cast<std::uint8_t>(r0),
        static_cast<std::uint8_t>(r1 >> 8), static_cast<std::uint8_t>(r1)};
    m_bus.data(rows, sizeof rows);
    m_bus.command(cmd::MemoryWrite);
}

void St7735::fillRect(int x, int y, int w, int h, std::uint16_t color)
{
    Rect r;
    if (!clip(x, y, w, h, r)) {
        return;
    }
    beginWrite(r);
    m_bus.repeat(color, static_cast<std::uint32_t>(r.x1 - r.x0 + 1) * static_cast<std::uint32_t>(r.y1 - r.y0 + 1));
    m_bus.release();
}

void St7735::drawBitmap(int x, int y, int w, int h, const std::uint8_t *rows, int stride, std::uint16_t fg, std::uint16_t bg)
{
    Rect r;
    if (!clip(x, y, w, h, r)) {
        return;
    }
    beginWrite(r);
    const auto pixel = [&](int px, int py) {
        const int bx = px - x;
        const int by = py - y;
        return (rows[by * stride + bx / 8] & (0x80u >> (bx % 8))) != 0 ? fg : bg;
    };
    // Equal neighbours go out as one run.
    std::uint16_t run = 0;
    std::uint32_t length = 0;
    const auto emit = [&](std::uint16_t c) {
        if (length != 0 && c != run) {
            m_bus.repeat(run, length);
            length = 0;
        }
        run = c;
        ++length;
    };
    if (m_geometry.axesSwapped) {
        for (int px = r.x0; px <= r.x1; ++px) {
            for (int py = r.y0; py <= r.y1; ++py) {
                emit(pixel(px, py));
            }
        }
    } else {
        for (int py = r.y0; py <= r.y1; ++py) {
            for (int px = r.x0; px <= r.x1; ++px) {
                emit(pixel(px, py));
            }
        }
    }
    if (length != 0) {
        m_bus.repeat(run, length);
    }
    m_bus.release();
}

} // namespace dmm::drivers::display
