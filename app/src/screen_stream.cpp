// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Henri Lavikainen

#include "screen_stream.h"

#include "dmm/lib/format.h"
#include "dmm/services/log.h"

#include <atomic>

namespace dmm::app::ui {

namespace log = services::log;

namespace {

// Bitmap bytes per log line, so a line stays well inside MaxLineLength.
constexpr int BytesPerLine = 40;

// What was asked: the request in the low byte, the seconds above it.
std::atomic<std::uint32_t> asked{0};

} // namespace

void ScreenStream::fillRect(int x, int y, int w, int h, services::Color color)
{
    m_display.fillRect(x, y, w, h, color);
    if (!m_streaming) {
        return;
    }
    waitForRoom();
    log::info("scr") << "f " << m_line++ << ' ' << x << ' ' << y << ' ' << w << ' ' << h << ' ' << lib::hex(color, 4);
    m_drew = true;
}

void ScreenStream::drawBitmap(int x, int y, int w, int h, const std::uint8_t *rows, int stride, services::Color fg,
    services::Color bg)
{
    m_display.drawBitmap(x, y, w, h, rows, stride, fg, bg);
    if (!m_streaming) {
        return;
    }
    const int rowBytes = (w + 7) / 8;
    const int perLine = rowBytes >= BytesPerLine ? 1 : BytesPerLine / rowBytes;
    for (int first = 0; first < h; first += perLine) {
        const int n = h - first < perLine ? h - first : perLine;
        waitForRoom();
        auto line = log::info("scr");
        line << "b " << m_line++ << ' ' << x << ' ' << y + first << ' ' << w << ' ' << n << ' ' << lib::hex(fg, 4) << ' '
             << lib::hex(bg, 4) << ' ';
        for (int r = first; r < first + n; ++r) {
            for (int b = 0; b < rowBytes; ++b) {
                line << lib::hex(rows[r * stride + b], 2);
            }
        }
    }
    m_drew = true;
}

void ScreenStream::stream(bool on, std::uint32_t seconds)
{
    m_timed = on && seconds > 0;
    m_until = os::nowMs() + seconds * 1000;
    if (on == m_streaming) {
        return;
    }
    m_streaming = on;
    m_drew = false;
    if (on) {
        m_line = 0;
        log::info("scr") << "start " << width() << ' ' << height();
    } else {
        log::info("scr") << "stop " << m_line;
    }
}

void ScreenStream::ask(Request request, std::uint16_t seconds)
{
    asked.store(static_cast<std::uint32_t>(request) | static_cast<std::uint32_t>(seconds) << 8);
}

bool ScreenStream::passDone()
{
    if (m_streaming && m_drew) {
        waitForRoom();
        log::info("scr") << "e " << m_line++;
        m_drew = false;
    }
    if (m_shot) {
        m_shot = false;
        stream(false);
        return false;
    }
    if (m_timed && static_cast<std::int32_t>(os::nowMs() - m_until) >= 0) {
        stream(false);
    }
    const std::uint32_t request = asked.exchange(0);
    const auto seconds = static_cast<std::uint16_t>(request >> 8);
    switch (static_cast<Request>(request & 0xFF)) {
    case Request::Shot:
        // Within a running stream the whole screen is one more pass.
        if (!m_streaming) {
            m_shot = true;
            stream(true);
        }
        return true;
    case Request::On:
        stream(true);
        break;
    case Request::Watch:
        stream(true, seconds);
        break;
    case Request::Off:
        stream(false);
        break;
    case Request::None:
        break;
    }
    return false;
}

void ScreenStream::waitForRoom()
{
    for (int i = 0; i < 100 && log::room() < log::MaxLineLength + 1; ++i) {
        os::sleepMs(1);
    }
}

} // namespace dmm::app::ui
