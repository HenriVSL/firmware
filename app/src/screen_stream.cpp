// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Henri Lavikainen

#include "screen_stream.h"

#include "dmm/lib/format.h"
#include "dmm/services/font.h"
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
    if (m_text) {
        addGlyph(x, y, w, h, rows, fg, bg);
    }
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

void ScreenStream::addGlyph(int x, int y, int w, int h, const std::uint8_t *rows, services::Color fg, services::Color bg)
{
    namespace fonts = services::fonts;
    for (const services::Font *font : {&fonts::Spleen5x8, &fonts::Spleen6x12, &fonts::Spleen8x16, &fonts::Spleen12x24}) {
        const std::size_t glyphBytes = font->bytesPerRow() * font->height;
        const std::size_t tableBytes = (font->last - font->first + 1u) * glyphBytes;
        if (w != font->width || h != font->height || rows < font->glyphs || rows >= font->glyphs + tableBytes) {
            continue;
        }
        const auto codePoint = static_cast<std::uint32_t>(font->first + static_cast<std::size_t>(rows - font->glyphs) / glyphBytes);
        if (m_run.chars.size() == 0 || y != m_run.y || x != m_run.end || h != m_run.height || fg != m_run.fg ||
            bg != m_run.bg || m_run.chars.size() + 2 > m_run.chars.capacity()) {
            endRun();
            m_run.x = x;
            m_run.y = y;
            m_run.height = h;
            m_run.fg = fg;
            m_run.bg = bg;
        }
        if (codePoint < 0x80) {
            m_run.chars << static_cast<char>(codePoint);
        } else {
            m_run.chars << static_cast<char>(0xC0 | codePoint >> 6) << static_cast<char>(0x80 | (codePoint & 0x3F));
        }
        m_run.end = x + w;
        return;
    }
}

void ScreenStream::endRun()
{
    if (m_run.chars.size() == 0) {
        return;
    }
    waitForRoom();
    log::info("scr") << "t " << m_run.x << ' ' << m_run.y << ' ' << m_run.height << ' ' << lib::hex(m_run.fg, 4) << ' '
                     << lib::hex(m_run.bg, 4) << ' ' << m_run.chars.view();
    m_run.chars.clear();
}

bool ScreenStream::passDone()
{
    if (m_text) {
        endRun();
        m_text = false;
        log::info("scr") << "text end";
    }
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
    case Request::Text:
        m_text = true;
        log::info("scr") << "text";
        return true;
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
