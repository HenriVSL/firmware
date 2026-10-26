// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tommi Tauriainen

#pragma once

#include "dmm/services/input.h"

namespace board {

/// Keypad, side keys, PTT, channel knob and volume knob.
class BoardInput
{
public:
    /// Sets up the pins and the knob interrupt. Call once, before
    /// the scheduler starts.
    void configure();

    dmm::services::KeyMask readKeys();
    std::int32_t encoderPosition();
    int encoderCountsPerDetent() const;
    int volumeKnob();
    /// As printed on the radio: P1, P2 on the front, PF1, PF2 on the side,
    /// PF3 on top.
    std::string_view keyLabel(dmm::services::Key key) const;

    /// From the EXTI5_9 interrupt.
    static void onEncoderEdge();
};

} // namespace board
