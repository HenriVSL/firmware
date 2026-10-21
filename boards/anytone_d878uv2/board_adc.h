// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tommi Tauriainen

#pragma once

#include "dmm/hal/gd32f30x/adc.h"
#include "dmm/os/os.h"

#include <cstdint>

namespace board {

/// ADC0, shared by the input task (volume knob) and the system task
/// (battery). Each reading takes the lock.
class SharedAdc
{
public:
    void start() { m_adc.start(); }

    std::uint16_t read(unsigned channel)
    {
        m_lock.lock();
        const std::uint16_t value = m_adc.read(channel);
        m_lock.unlock();
        return value;
    }

private:
    dmm::hal::gd32f30x::Adc0 m_adc;
    dmm::os::Mutex m_lock;
};

SharedAdc &adc();

} // namespace board
