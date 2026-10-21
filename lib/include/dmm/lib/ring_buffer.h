// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tommi Tauriainen

#pragma once

#include <array>
#include <cstddef>
#include <optional>

namespace dmm::lib {

/// Fixed-capacity FIFO. Not thread-safe; callers that share one across
/// tasks guard it themselves.
template <typename T, std::size_t N>
class RingBuffer
{
    static_assert(N > 0);

public:
    /// Appends a copy of @p value. Returns false, and changes nothing, when full.
    bool push(const T &value)
    {
        if (full()) {
            return false;
        }
        m_data[(m_head + m_count) % N] = value;
        ++m_count;
        return true;
    }

    /// Removes and returns the oldest element, or nothing when empty.
    std::optional<T> pop()
    {
        if (empty()) {
            return std::nullopt;
        }
        T value = m_data[m_head];
        m_head = (m_head + 1) % N;
        --m_count;
        return value;
    }

    /// The oldest element. Undefined when empty.
    const T &front() const { return m_data[m_head]; }

    void clear() { m_head = m_count = 0; }
    std::size_t size() const { return m_count; }
    std::size_t freeSpace() const { return N - m_count; }
    static constexpr std::size_t capacity() { return N; }
    bool empty() const { return m_count == 0; }
    bool full() const { return m_count == N; }

private:
    std::array<T, N> m_data{};
    std::size_t m_head = 0;
    std::size_t m_count = 0;
};

} // namespace dmm::lib
