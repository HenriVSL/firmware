// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tommi Tauriainen

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace dmm::lib {

/// SHA-256 (FIPS 180-4), incremental, no heap.
class Sha256
{
public:
    using Digest = std::array<std::uint8_t, 32>;

    Sha256() { reset(); }
    void reset();
    void update(std::span<const std::uint8_t> data);
    void update(std::string_view text);
    /// Finishes the hash. The object must be reset() before reuse.
    Digest finish();

    /// One-shot hash of @p data.
    static Digest of(std::span<const std::uint8_t> data);
    /// Lower-case hex text of @p d, 64 characters.
    static std::array<char, 64> toHex(const Digest &d);

private:
    void processBlock(const std::uint8_t *p);

    std::array<std::uint32_t, 8> m_state{};
    std::array<std::uint8_t, 64> m_buf{};
    std::size_t m_bufLen = 0;
    std::uint64_t m_total = 0;
};

} // namespace dmm::lib
