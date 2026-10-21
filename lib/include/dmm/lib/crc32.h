// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tommi Tauriainen

#pragma once

#include <cstddef>
#include <cstdint>

namespace dmm::lib {

/// CRC-32 as in zlib, PNG and Ethernet (reflected, polynomial 0xEDB88320).
/// Start with 0 and feed the previous result to continue over more data.
constexpr std::uint32_t crc32(const std::uint8_t *data, std::size_t size, std::uint32_t crc = 0)
{
    crc = ~crc;
    for (std::size_t i = 0; i < size; ++i) {
        crc ^= data[i];
        for (int bit = 0; bit < 8; ++bit) {
            crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
        }
    }
    return ~crc;
}

} // namespace dmm::lib
