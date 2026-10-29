// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Henri Lavikainen

#pragma once

#include <cstddef>
#include <cstdint>

namespace dmm::services {

/// What one log entry can be.
enum class EventKind : std::uint8_t
{
    CallIn,     // a: caller ID, b: talkgroup or private target, value: seconds; group: talkgroup call
    CallOut,    // a: 0 DMR, 1 FM; b: talkgroup or private target, or kHz on FM; value: seconds
    RoamMove,   // a: the repeater's place in the roaming zone, from 1
    RoamLost,   // a: the repeater's place in the roaming zone, from 1
    AprsBeacon, // a beacon went out
    GpsFix,     // the receiver has a position
    GpsLost,    // the receiver lost it
};

/// A short log of what the radio did, the last @p N entries, for a screen
/// that lists them. Entries hold numbers only (IDs, talkgroups, seconds);
/// names are looked up when a list is drawn. A log may take several
/// kinds, and a list may show only some of them.
template <std::size_t N>
class EventLog
{
public:
    static constexpr std::size_t Capacity = N;

    /// A set of kinds, for a list that shows only some.
    using Kinds = std::uint32_t;
    static constexpr Kinds all() { return ~Kinds{0}; }
    static constexpr Kinds only(EventKind k) { return Kinds{1} << static_cast<unsigned>(k); }

    /// 12 bytes: when (seconds on the radio's clock since start), the
    /// kind, a group flag, two 24-bit numbers and a 7-bit value.
    class Entry
    {
    public:
        std::uint32_t seconds() const { return m_seconds; }
        EventKind kind() const { return static_cast<EventKind>(m_a >> 24); }
        std::uint32_t a() const { return m_a & 0xFFFFFF; }
        std::uint32_t b() const { return m_b & 0xFFFFFF; }
        bool group() const { return (m_b >> 31) != 0; }
        /// 0..127; 127 also stands for anything larger.
        std::uint8_t value() const { return static_cast<std::uint8_t>(m_b >> 24 & 0x7F); }

    private:
        friend class EventLog;
        std::uint32_t m_seconds = 0;
        std::uint32_t m_a = 0;
        std::uint32_t m_b = 0;
    };

    /// Logs @p kind at @p seconds; the oldest entry gives way when full.
    void add(EventKind kind, std::uint32_t seconds, std::uint32_t a = 0, std::uint32_t b = 0, unsigned value = 0,
        bool group = false)
    {
        Entry &e = m_entries[m_next];
        e.m_seconds = seconds;
        e.m_a = static_cast<std::uint32_t>(kind) << 24 | (a & 0xFFFFFF);
        e.m_b = packB(b, value, group);
        m_next = (m_next + 1) % N;
        if (m_count < N) {
            ++m_count;
        }
    }

    /// Sets the value of the newest entry of @p kind, as a call that is
    /// still going on gets longer.
    void update(EventKind kind, unsigned value)
    {
        for (std::size_t i = 0; i < m_count; ++i) {
            Entry &e = m_entries[(m_next + N - 1 - i) % N];
            if (e.kind() == kind) {
                e.m_b = packB(e.b(), value, e.group());
                return;
            }
        }
    }

    /// How many entries of the @p kinds there are.
    std::size_t count(Kinds kinds = all()) const
    {
        std::size_t n = 0;
        for (std::size_t i = 0; i < m_count; ++i) {
            if ((kinds & only(m_entries[i].kind())) != 0) {
                ++n;
            }
        }
        return n;
    }

    /// The @p index-th newest entry of the @p kinds; nullptr past the end.
    const Entry *newest(std::size_t index, Kinds kinds = all()) const
    {
        for (std::size_t i = 0; i < m_count; ++i) {
            const Entry &e = m_entries[(m_next + N - 1 - i) % N];
            if ((kinds & only(e.kind())) == 0) {
                continue;
            }
            if (index == 0) {
                return &e;
            }
            --index;
        }
        return nullptr;
    }

private:
    static std::uint32_t packB(std::uint32_t b, unsigned value, bool group)
    {
        const std::uint32_t v = value > 127 ? 127 : value;
        return (group ? 0x80000000u : 0) | v << 24 | (b & 0xFFFFFF);
    }

    Entry m_entries[N];
    std::size_t m_next = 0;
    std::size_t m_count = 0;
};

} // namespace dmm::services
