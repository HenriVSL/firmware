// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tommi Tauriainen

#include "sim_input.h"

void SimInput::setKey(dmm::services::Key key, bool down)
{
    if (down) {
        m_keys |= dmm::services::keyBit(key);
    } else {
        m_keys &= ~dmm::services::keyBit(key);
    }
}

void SimInput::turn(int clicks)
{
    m_position += clicks * CountsPerDetent;
}

void SimInput::setVolume(int value)
{
    m_volume = value < 0 ? 0 : value > 1000 ? 1000
                                            : value;
}
