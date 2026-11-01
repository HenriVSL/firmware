// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Henri Lavikainen

#pragma once

#include "sim_aprs_stations.h"

#include "dmm/services/ax25.h"

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <mutex>
#include <span>
#include <string>
#include <string_view>

/// A BT+APRS board that hears made-up stations near the simulated GPS
/// position (sim_aprs): it answers the start with "+READY", then sends
/// what it hears as the board does, a second into each slot it hears.
class SimAprsLink
{
public:
    void start()
    {
        std::lock_guard guard(m_lock);
        m_on = true;
        m_lastSlot = sim_aprs::slotAt(std::chrono::steady_clock::now() - std::chrono::seconds(1));
        put("\x06+READY\r\n");
    }
    void stop()
    {
        std::lock_guard guard(m_lock);
        m_on = false;
        m_out.clear();
    }
    std::size_t read(std::span<std::uint8_t> data)
    {
        std::lock_guard guard(m_lock);
        const std::int64_t slot = sim_aprs::slotAt(std::chrono::steady_clock::now() - std::chrono::seconds(1));
        if (m_on && slot != m_lastSlot) {
            m_lastSlot = slot;
            if (sim_aprs::boardHears(slot)) {
                hear(slot);
            }
        }
        std::size_t n = 0;
        while (n < data.size() && !m_out.empty()) {
            data[n++] = m_out.front();
            m_out.pop_front();
        }
        return n;
    }
    /// Echoes each text line sent to it as "+SIM:<line>".
    void write(std::span<const std::uint8_t> data)
    {
        std::lock_guard guard(m_lock);
        for (std::uint8_t b : data) {
            if (b == '\n') {
                if (m_on) {
                    put("+SIM:");
                    put(m_typed);
                    put("\r\n");
                }
                m_typed.clear();
            } else if (b >= ' ' && b <= '~' && m_typed.size() < 64) {
                m_typed += static_cast<char>(b);
            }
        }
    }
    std::uint32_t lostBytes() const { return 0; }

private:
    void put(std::string_view text) { m_out.insert(m_out.end(), text.begin(), text.end()); }

    void hear(std::int64_t slot)
    {
        std::array<std::uint8_t, dmm::services::ax25::MaxFrameBytes> bytes{};
        const std::size_t n = sim_aprs::frame(slot, bytes);
        put("+APRS:~");
        m_out.insert(m_out.end(), bytes.begin(), bytes.begin() + static_cast<std::ptrdiff_t>(n));
        m_out.push_back(0x7E);
    }

    std::mutex m_lock;
    std::deque<std::uint8_t> m_out;
    std::string m_typed;
    bool m_on = false;
    std::int64_t m_lastSlot = 0;
};
