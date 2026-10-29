// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Henri Lavikainen

#pragma once

#include <cstddef>
#include <cstdint>

namespace dmm::services {

/// A short log of what the radio did, kept for the screens that list it.
/// One buffer serves every such list: each entry has a kind, and a list
/// shows the kinds it wants. Entries hold numbers only (IDs, talkgroups,
/// seconds); names are looked up when a list is drawn.
class EventLog
{
public:
    static constexpr std::size_t Capacity = 32;

    enum class Kind : std::uint8_t
    {
        CallIn,     // a: caller ID, b: talkgroup or private target, value: seconds; group: talkgroup call
        CallOut,    // b: talkgroup or private target, value: seconds; group: talkgroup call
        RoamMove,   // a: the repeater's place in the roaming zone, from 1
        RoamLost,   // a: the repeater's place in the roaming zone, from 1
        AprsBeacon, // a beacon went out
        GpsFix,     // the receiver has a position
        GpsLost,    // the receiver lost it
    };

    /// A set of kinds, for a list that shows only some.
    using Kinds = std::uint32_t;
    static constexpr Kinds all() { return ~Kinds{0}; }
    static constexpr Kinds only(Kind k) { return Kinds{1} << static_cast<unsigned>(k); }

    /// 12 bytes: when (seconds on the radio's clock since start), the
    /// kind, a group flag, two 24-bit numbers and a 7-bit value.
    class Entry
    {
    public:
        std::uint32_t seconds() const { return m_seconds; }
        Kind kind() const { return static_cast<Kind>(m_a >> 24); }
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
    void add(Kind kind, std::uint32_t seconds, std::uint32_t a = 0, std::uint32_t b = 0, unsigned value = 0,
        bool group = false);
    /// Sets the value of the newest entry of @p kind, as a call that is
    /// still going on gets longer.
    void update(Kind kind, unsigned value);

    /// How many entries of the @p kinds there are.
    std::size_t count(Kinds kinds = all()) const;
    /// The @p index-th newest entry of the @p kinds; nullptr past the end.
    const Entry *newest(std::size_t index, Kinds kinds = all()) const;

private:
    Entry m_entries[Capacity];
    std::size_t m_next = 0;
    std::size_t m_count = 0;
};

} // namespace dmm::services
