// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tommi Tauriainen

#pragma once

#include <cstdint>

namespace dmm::services {

/// Transmit power levels, lowest first. The numbers give the order only;
/// stored formats map their own values.
enum class PowerLevel : std::uint8_t
{
    Low = 0,
    Mid = 1,
    High = 2,
    Turbo = 3,
};

} // namespace dmm::services
