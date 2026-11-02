// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Henri Lavikainen

#pragma once

#include "dmm/os/os.h"
#include "dmm/services/ax25.h"
#include "dmm/services/log.h"
#include "dmm/services/rtty.h"
#include "dmm/services/rtty_modulator.h"

#include <cstdint>
#include <span>

/// Logs each frame or text instead of sending it: a frame is done at once,
/// a text after the time it would take on air.
class SimModem
{
public:
    static constexpr std::uint32_t SampleRateHz = 32'000;

    bool send(std::span<const std::uint8_t> frame, std::uint16_t leadMs, std::uint8_t levelPercent)
    {
        dmm::services::ax25::Frame f;
        auto line = dmm::services::log::info("modem");
        if (dmm::services::ax25::decode(frame, f)) {
            dmm::services::ax25::writeText(line, f);
        } else {
            line << "a frame that does not decode";
        }
        line << " (" << leadMs << " ms lead, level " << levelPercent << " %)";
        return true;
    }

    bool sendRtty(std::span<const std::uint8_t> codes, const dmm::services::RttyModulator::Config &config,
        std::uint16_t leadMs, std::uint16_t tailMs, std::uint8_t levelPercent)
    {
        if (sending()) {
            return false;
        }
        dmm::services::rtty::Ita2Decoder decoder;
        auto line = dmm::services::log::info("modem");
        line << "RTTY \"";
        for (const std::uint8_t c : codes) {
            const char ch = decoder.decode(c);
            if (ch >= ' ' && ch <= '~') {
                line << ch;
            } else if (ch == '\n') {
                line << "\\n";
            }
        }
        line << "\" (" << config.markHz << " Hz mark, level " << levelPercent << " %)";
        m_until = dmm::os::nowMs() + dmm::services::RttyModulator::airMs(codes.size(), config.baud, leadMs, tailMs);
        m_busy = true;
        return true;
    }

    void stop() { m_busy = false; }

    bool sending()
    {
        if (m_busy && static_cast<std::int32_t>(dmm::os::nowMs() - m_until) >= 0) {
            m_busy = false;
        }
        return m_busy;
    }

private:
    dmm::os::Ticks m_until = 0;
    bool m_busy = false;
};
