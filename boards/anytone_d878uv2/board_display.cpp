// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tommi Tauriainen

#include "board_display.h"

#include "panel_bus.h"
#include "pins.h"

#include "dmm/lib/format.h"
#include "dmm/lib/strings.h"
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
    m_mirror.fill(x, y, w, h, color);
}

void BoardDisplay::drawBitmap(int x, int y, int w, int h, const std::uint8_t *rows, int stride,
    dmm::services::Color fg, dmm::services::Color bg)
{
    PanelPinsGuard guard;
    m_panel.drawBitmap(x, y, w, h, rows, stride, fg, bg);
    m_mirror.bitmap(x, y, w, h, rows, stride, fg, bg);
}

void BoardDisplay::setBacklight(int level)
{
    level = level < 0 ? 0 : level > BacklightLevels ? BacklightLevels
                                                    : level;
    m_pwm.setDuty(m_strap ? BacklightDutyStrap1[level] : BacklightDutyStrap0[level]);
}

bool BoardDisplay::debugCommand(std::string_view text)
{
    namespace log = dmm::services::log;
    using Mirror = decltype(m_mirror);
    auto number = [&text](int &out) {
        while (!text.empty() && text.front() == ' ') {
            text.remove_prefix(1);
        }
        const auto r = std::from_chars(text.data(), text.data() + text.size(), out);
        if (r.ec != std::errc{}) {
            return false;
        }
        text.remove_prefix(static_cast<std::size_t>(r.ptr - text.data()));
        return true;
    };
    if (text == "colours") {
        auto line = log::info("display");
        line << "screen colours " << m_mirror.colourCount();
        for (const auto c : m_mirror.colours()) {
            line << ' ' << dmm::lib::hex(c, 4);
        }
        return true;
    }
    if (text == "flicker") {
        std::uint32_t worst = 0;
        const std::uint32_t n = m_mirror.takeFlicker(worst);
        log::info("display") << "screen flicker " << n << ", worst frame " << worst;
        return true;
    }
    if (dmm::lib::startsWith(text, "rows ")) {
        text.remove_prefix(5);
        int first = 0;
        int count = 0;
        if (!number(first) || !number(count) || first < 0 || count < 1 || first + count > Mirror::Height) {
            return false;
        }
        constexpr char Hex[] = "0123456789abcdef";
        for (int y = first; y < first + count; ++y) {
            for (int half = 0; half < 2; ++half) {
                char buf[Mirror::Width / 2 + 1];
                for (int i = 0; i < Mirror::Width / 2; ++i) {
                    buf[i] = Hex[m_mirror.at(half * (Mirror::Width / 2) + i, y)];
                }
                buf[Mirror::Width / 2] = '\0';
                log::info("display") << "screen " << y << ' ' << half << ' ' << buf;
            }
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
