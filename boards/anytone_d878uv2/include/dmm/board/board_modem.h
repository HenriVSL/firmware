// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Henri Lavikainen

#pragma once

#include "dmm/services/rtty_modulator.h"

#include <cstdint>
#include <span>

namespace board {

/// Packet radio AFSK and RTTY on the beep pin, which also reaches the
/// transmitter's modulation input when the microphone is muted.
class BoardModem
{
public:
    /// The beep pin's sample rate.
    static constexpr std::uint32_t SampleRateHz = 32'000;

    bool send(std::span<const std::uint8_t> frame, std::uint16_t leadMs, std::uint8_t levelPercent);
    bool sendRtty(std::span<const std::uint8_t> codes, const dmm::services::RttyModulator::Config &config,
        std::uint16_t leadMs, std::uint16_t tailMs, std::uint8_t levelPercent);
    void stop();
    bool sending();

private:
    static void play(int (*source)());
};

} // namespace board
