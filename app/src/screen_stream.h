// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Henri Lavikainen

#pragma once

#include "dmm/lib/format.h"
#include "dmm/os/os.h"
#include "dmm/services/display.h"

#include <cstdint>

namespace dmm::app::ui {

/// Passes the drawing on to a display and, while streaming, also writes it
/// to the log, so a PC can put pictures of the screen together for tests
/// (tools/screen_picture). Log lines, module "scr":
///   start <width> <height>
///   f <n> <x> <y> <w> <h> <colour>
///   b <n> <x> <y> <w> <h> <fg> <bg> <rows>
///   e <n>                  after each pass of the ui that drew
///   stop <n>
/// Colours are RGB565 in hex; a bitmap's rows are hex bytes, the leftmost
/// pixel in the top bit, each row whole bytes. @p n counts the lines, so a
/// lost one shows. Drawing waits for room in the log rather than lose a
/// line.
///
/// Asked for the screen's text, it writes each run of characters drawn
/// side by side in one font and colour as the ui draws the whole screen:
///   text
///   t <x> <y> <font height> <fg> <bg> <characters, UTF-8>
///   text end
class ScreenStream final : public services::Display
{
public:
    explicit ScreenStream(services::Display &display)
        : m_display(display)
    {
    }

    void start() override { m_display.start(); }
    int width() const override { return m_display.width(); }
    int height() const override { return m_display.height(); }
    void fillRect(int x, int y, int w, int h, services::Color color) override;
    void drawBitmap(int x, int y, int w, int h, const std::uint8_t *rows, int stride, services::Color fg,
        services::Color bg) override;
    void setBacklight(int level) override { m_display.setBacklight(level); }
    void setPanelOn(bool on) override { m_display.setPanelOn(on); }

    enum class Request : std::uint8_t
    {
        None,
        Shot,  // one whole screen
        On,    // from now on
        Watch, // for some seconds
        Off,
        Text, // the text on the whole screen
    };
    /// Asks for a picture or a change in streaming (console, for tests);
    /// any task. The ui carries it out at the end of its next pass.
    static void ask(Request request, std::uint16_t seconds = 0);

    /// Starts or stops streaming; @p seconds > 0 stops by itself after that
    /// long.
    void stream(bool on, std::uint32_t seconds = 0);
    bool streaming() const { return m_streaming; }
    /// The ui's pass is over: ends the pass's lines, the stream when its
    /// time is up, and carries out what was asked. True when a whole
    /// screen is wanted: the ui then draws all of it and calls this again.
    bool passDone();

private:
    void waitForRoom();
    void addGlyph(int x, int y, int w, int h, const std::uint8_t *rows, services::Color fg, services::Color bg);
    void endRun();

    services::Display &m_display;
    bool m_streaming = false;
    bool m_drew = false;
    std::uint16_t m_line = 0;
    os::Ticks m_until = 0;
    bool m_timed = false;
    bool m_shot = false;
    // The text pass and the run of characters it is collecting.
    bool m_text = false;
    struct Run
    {
        int x = 0;
        int y = 0;
        int end = 0;
        int height = 0;
        services::Color fg = 0;
        services::Color bg = 0;
        lib::StaticString<72> chars;
    } m_run;
};

} // namespace dmm::app::ui
