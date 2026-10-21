// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tommi Tauriainen

#pragma once

#include "dmm/services/input.h"

#include <atomic>

/// Simulated keys and knobs, set from the GUI thread and read by the app.
class SimInput final : public dmm::services::InputDevices
{
public:
    void setKey(dmm::services::Key key, bool down);
    /// Turns the channel knob by @p clicks (positive = clockwise).
    void turn(int clicks);
    /// 0..1000.
    void setVolume(int value);

    dmm::services::KeyMask readKeys() override { return m_keys.load(); }
    std::int32_t encoderPosition() override { return m_position.load(); }
    int encoderCountsPerDetent() const override { return CountsPerDetent; }
    int volumeKnob() override { return m_volume.load(); }

private:
    static constexpr int CountsPerDetent = 2;

    std::atomic<dmm::services::KeyMask> m_keys{0};
    std::atomic<std::int32_t> m_position{0};
    std::atomic<int> m_volume{500};
};
