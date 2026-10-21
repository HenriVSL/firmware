// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tommi Tauriainen

#pragma once

#include <cstddef>
#include <memory>
#include <new>
#include <span>
#include <type_traits>
#include <utility>

namespace dmm::lib {

/// Vector with fixed capacity and inline storage. Operations that would
/// exceed the capacity return false instead of growing.
template <typename T, std::size_t N>
class FixedVector
{
public:
    FixedVector() = default;
    FixedVector(const FixedVector &other)
    {
        for (const T &v : other) {
            pushBack(v);
        }
    }
    FixedVector &operator=(const FixedVector &other)
    {
        if (this != &other) {
            clear();
            for (const T &v : other) {
                pushBack(v);
            }
        }
        return *this;
    }
    ~FixedVector() { clear(); }

    bool pushBack(const T &value) { return emplaceBack(value); }

    template <typename... A>
    bool emplaceBack(A &&...args)
    {
        if (m_size == N) {
            return false;
        }
        std::construct_at(ptr(m_size), std::forward<A>(args)...);
        ++m_size;
        return true;
    }

    void popBack()
    {
        if (m_size > 0) {
            --m_size;
            std::destroy_at(ptr(m_size));
        }
    }

    void clear()
    {
        while (m_size > 0) {
            popBack();
        }
    }

    T &operator[](std::size_t i) { return *ptr(i); }
    const T &operator[](std::size_t i) const { return *ptr(i); }
    T *data() { return ptr(0); }
    const T *data() const { return ptr(0); }
    T *begin() { return ptr(0); }
    T *end() { return ptr(m_size); }
    const T *begin() const { return ptr(0); }
    const T *end() const { return ptr(m_size); }
    std::span<T> span() { return {data(), m_size}; }
    std::span<const T> span() const { return {data(), m_size}; }

    std::size_t size() const { return m_size; }
    static constexpr std::size_t capacity() { return N; }
    bool empty() const { return m_size == 0; }
    bool full() const { return m_size == N; }

private:
    T *ptr(std::size_t i) { return std::launder(reinterpret_cast<T *>(m_storage) + i); }
    const T *ptr(std::size_t i) const { return std::launder(reinterpret_cast<const T *>(m_storage) + i); }

    alignas(T) std::byte m_storage[sizeof(T) * N];
    std::size_t m_size = 0;
};

} // namespace dmm::lib
