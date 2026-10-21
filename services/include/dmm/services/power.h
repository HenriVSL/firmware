// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tommi Tauriainen

#pragma once

#include "dmm/os/os.h"

#include <cstdint>
#include <string_view>

namespace dmm::services::power {

/// Why the radio must stay fully awake. Battery save may start only when
/// none is held. Each user of the receiver, the CPU or the display holds
/// its own reason while it needs it.
enum class AwakeReason : std::uint8_t
{
    Transmit,
    Carrier,
    Call,
    Scan,
    Tone,
    UsbLink,
    Display,
    Count,
};

std::string_view name(AwakeReason reason);

void hold(AwakeReason reason);
void release(AwakeReason reason);
bool isHeld(AwakeReason reason);

/// Bit n set = reason n held.
std::uint32_t heldMask();

/// Milliseconds since the last reason was released; 0 while any is held.
os::Ticks idleForMs();

} // namespace dmm::services::power
