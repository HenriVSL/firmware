// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Henri Lavikainen

#include "dmm/services/event_log.h"

namespace dmm::services {

namespace {

std::uint32_t packB(std::uint32_t b, unsigned value, bool group)
{
    const std::uint32_t v = value > 127 ? 127 : value;
    return (group ? 0x80000000u : 0) | v << 24 | (b & 0xFFFFFF);
}

} // namespace

void EventLog::add(Kind kind, std::uint32_t seconds, std::uint32_t a, std::uint32_t b, unsigned value, bool group)
{
    Entry &e = m_entries[m_next];
    e.m_seconds = seconds;
    e.m_a = static_cast<std::uint32_t>(kind) << 24 | (a & 0xFFFFFF);
    e.m_b = packB(b, value, group);
    m_next = (m_next + 1) % Capacity;
    if (m_count < Capacity) {
        ++m_count;
    }
}

void EventLog::update(Kind kind, unsigned value)
{
    for (std::size_t i = 0; i < m_count; ++i) {
        Entry &e = m_entries[(m_next + Capacity - 1 - i) % Capacity];
        if (e.kind() == kind) {
            e.m_b = packB(e.b(), value, e.group());
            return;
        }
    }
}

std::size_t EventLog::count(Kinds kinds) const
{
    std::size_t n = 0;
    for (std::size_t i = 0; i < m_count; ++i) {
        if ((kinds & only(m_entries[i].kind())) != 0) {
            ++n;
        }
    }
    return n;
}

const EventLog::Entry *EventLog::newest(std::size_t index, Kinds kinds) const
{
    for (std::size_t i = 0; i < m_count; ++i) {
        const Entry &e = m_entries[(m_next + Capacity - 1 - i) % Capacity];
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

} // namespace dmm::services
