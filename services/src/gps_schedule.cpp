// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Henri Lavikainen

#include "dmm/services/gps_schedule.h"

namespace dmm::services {

namespace {

// a is at or after b on a clock that wraps.
bool reached(std::uint32_t a, std::uint32_t b)
{
    return static_cast<std::int32_t>(a - b) >= 0;
}

// The later of two times on a clock that wraps.
std::uint32_t later(std::uint32_t a, std::uint32_t b)
{
    return reached(a, b) ? a : b;
}

} // namespace

void GpsSchedule::setNeed(Client client, std::uint32_t intervalMs)
{
    m_needs[static_cast<int>(client)] = intervalMs;
}

void GpsSchedule::setEnabled(bool on)
{
    m_enabled = on;
    m_fixSinceWake = false;
    m_retryPending = false;
    m_tryNow = on;
}

std::uint32_t GpsSchedule::shortestNeed() const
{
    std::uint32_t shortest = 0;
    for (std::uint32_t n : m_needs) {
        if (n != 0 && (shortest == 0 || n < shortest)) {
            shortest = n;
        }
    }
    return shortest;
}

bool GpsSchedule::continuous() const
{
    const std::uint32_t need = shortestNeed();
    return !m_powerSave || (need != 0 && need <= ContinuousUpToMs);
}

void GpsSchedule::fix(std::uint32_t now)
{
    if (!m_fixSinceWake) {
        // How long this start took, plus a margin, is the next lead.
        const std::uint32_t took = (now - m_wokeAt) + (now - m_wokeAt) / 4;
        m_leadMs = took < MinLeadMs ? MinLeadMs : took > MaxLeadMs ? MaxLeadMs
                                                                   : took;
        m_firstFixAt = now;
    }
    m_fixSinceWake = true;
    m_lastFixAt = now;
    m_everFixed = true;
    m_retryPending = false;
}

std::uint32_t GpsSchedule::wakeAt() const
{
    const std::uint32_t need = shortestNeed();
    std::uint32_t at = m_sleptAt + MinSleepMs;
    if (m_everFixed && need != 0) {
        // The next position is due a need after the last one; start a lead
        // earlier. A need that shrinks while asleep moves this earlier.
        at = later(at, m_lastFixAt + need - m_leadMs);
    }
    if (m_retryPending) {
        at = later(at, m_retryAt);
    }
    return at;
}

GpsSchedule::Action GpsSchedule::powerOn(std::uint32_t now)
{
    m_wokeAt = now;
    m_fixSinceWake = false;
    m_tryNow = false;
    return Action::PowerOn;
}

GpsSchedule::Action GpsSchedule::powerOff(std::uint32_t now)
{
    m_sleptAt = now;
    return Action::PowerOff;
}

GpsSchedule::Action GpsSchedule::update(std::uint32_t now, bool powered)
{
    if (!m_enabled) {
        return powered ? Action::PowerOff : Action::None;
    }
    if (continuous()) {
        return powered ? Action::None : powerOn(now);
    }
    const std::uint32_t need = shortestNeed();
    if (powered) {
        if (m_fixSinceWake) {
            return reached(now, m_firstFixAt + SettleMs) ? powerOff(now) : Action::None;
        }
        if (reached(now, m_wokeAt + GiveUpMs)) {
            // No fix this time (no sky, perhaps): try again a need later.
            m_retryAt = now + (need > MinSleepMs ? need : MinSleepMs);
            m_retryPending = true;
            return powerOff(now);
        }
        return Action::None;
    }
    if (need == 0) {
        return Action::None;
    }
    if (m_tryNow || (!m_everFixed && !m_retryPending) || reached(now, wakeAt())) {
        return powerOn(now);
    }
    return Action::None;
}

} // namespace dmm::services
