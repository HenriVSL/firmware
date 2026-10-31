// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tommi Tauriainen

#include "board_display.h"

#include "panel_bus.h"
#include "pins.h"

#include "dmm/lib/format.h"
#include "dmm/lib/strings.h"
#include "dmm/os/os.h"
#include "dmm/services/log.h"

#include <charconv>

namespace board {

namespace hal = dmm::hal::gd32f30x;

namespace {

void sleepMs(unsigned ms)
{
    dmm::os::sleepMs(ms);
}

} // namespace

// Mounted turned: screen x runs along the controller's rows, which run
// mirrored; colours in BGR order.
BoardDisplay::BoardDisplay(bool panelStrap)
    : m_strap(panelStrap), m_panel(lcdBus(), {160, 128, true, 0x88}), m_pwm(BacklightChannel)
{
}

void BoardDisplay::start()
{
    dmm::services::log::info("display") << "panel strap " << int{m_strap};

    hal::write(Backlight, false); // dark until the PWM takes over
    hal::configure(Backlight, hal::PinMode::Output);
    m_pwm.start(BacklightPrescaler, BacklightPeriod);
    hal::configure(Backlight, hal::PinMode::Alternate, hal::PinSpeed::Medium);

    PanelPinsGuard guard;
    m_panel.init(sleepMs);
}

void BoardDisplay::fillRect(int x, int y, int w, int h, dmm::services::Color color)
{
    PanelPinsGuard guard;
    m_panel.fillRect(x, y, w, h, color);
    if (m_streaming) {
        waitForLogRoom();
        dmm::services::log::info("scr") << "f " << m_streamLine++ << ' ' << x << ' ' << y << ' ' << w << ' ' << h
                                        << ' ' << dmm::lib::hex(color, 4);
        m_drew = true;
    }
}

void BoardDisplay::drawBitmap(int x, int y, int w, int h, const std::uint8_t *rows, int stride,
    dmm::services::Color fg, dmm::services::Color bg)
{
    PanelPinsGuard guard;
    m_panel.drawBitmap(x, y, w, h, rows, stride, fg, bg);
    if (!m_streaming) {
        return;
    }
    // As many rows to a line as fit in about 48 bytes.
    const int rowBytes = (w + 7) / 8;
    const int perLine = rowBytes >= 48 ? 1 : 48 / rowBytes;
    for (int first = 0; first < h; first += perLine) {
        const int n = h - first < perLine ? h - first : perLine;
        waitForLogRoom();
        auto line = dmm::services::log::info("scr");
        line << "b " << m_streamLine++ << ' ' << x << ' ' << y + first << ' ' << w << ' ' << n << ' '
             << dmm::lib::hex(fg, 4) << ' ' << dmm::lib::hex(bg, 4) << ' ';
        for (int r = first; r < first + n; ++r) {
            for (int b = 0; b < rowBytes; ++b) {
                line << dmm::lib::hex(rows[r * stride + b], 2);
            }
        }
    }
    m_drew = true;
}

void BoardDisplay::frameDone()
{
    if (!m_streaming) {
        return;
    }
    if (m_drew) {
        waitForLogRoom();
        dmm::services::log::info("scr") << "e " << m_streamLine++;
        m_drew = false;
    }
    if (m_streamUntil != 0 && static_cast<std::int32_t>(dmm::os::nowMs() - m_streamUntil) >= 0) {
        stream(false);
    }
}

void BoardDisplay::stream(bool on)
{
    if (on == m_streaming) {
        return;
    }
    m_streaming = on;
    m_drew = false;
    if (on) {
        m_streamLine = 0;
        dmm::services::log::info("scr") << "start " << width() << ' ' << height();
    } else {
        dmm::services::log::info("scr") << "stop " << m_streamLine;
    }
}

// Waits a little for the log task to make room, so no line is dropped.
void BoardDisplay::waitForLogRoom()
{
    for (int i = 0; i < 100 && dmm::services::log::room() < dmm::services::log::MaxLineLength + 1; ++i) {
        dmm::os::sleepMs(1);
    }
}

void BoardDisplay::setBacklight(int level)
{
    level = level < 0 ? 0 : level > BacklightLevels ? BacklightLevels
                                                    : level;
    m_pwm.setDuty(m_strap ? BacklightDutyStrap1[level] : BacklightDutyStrap0[level]);
}

bool BoardDisplay::debugCommand(std::string_view text)
{
    if (text == "stream on" || text == "stream off") {
        m_streamUntil = 0;
        stream(text == "stream on");
        return true;
    }
    if (dmm::lib::startsWith(text, "watch ")) {
        text.remove_prefix(6);
        int seconds = 0;
        const auto r = std::from_chars(text.data(), text.data() + text.size(), seconds);
        if (r.ec != std::errc{} || r.ptr != text.data() + text.size() || seconds < 1 || seconds > 600) {
            return false;
        }
        stream(true);
        // 0 means until off.
        m_streamUntil = dmm::os::nowMs() + static_cast<std::uint32_t>(seconds) * 1000;
        if (m_streamUntil == 0) {
            m_streamUntil = 1;
        }
        return true;
    }
    return false;
}

void BoardDisplay::setPanelOn(bool on)
{
    PanelPinsGuard guard;
    m_panel.setSleep(!on, sleepMs);
}

} // namespace board
