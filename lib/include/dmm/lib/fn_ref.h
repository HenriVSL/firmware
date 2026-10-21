// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tommi Tauriainen

#pragma once

#include <functional>
#include <memory>
#include <type_traits>
#include <utility>

namespace dmm::lib {

template <typename Signature>
class FnRef;

/// Non-owning reference to a callable: a pointer and a trampoline, no heap.
/// The referenced callable must outlive the FnRef.
template <typename R, typename... Args>
class FnRef<R(Args...)>
{
public:
    /// Refers to a callable object (lambda, functor). Implicit, like std::function.
    template <typename F>
        requires(!std::is_same_v<std::remove_cvref_t<F>, FnRef> &&
                    !std::is_pointer_v<std::remove_cvref_t<F>> &&
                    std::is_invocable_r_v<R, F &, Args...>)
    constexpr FnRef(F &&f) noexcept // NOLINT(bugprone-forwarding-reference-overload)
        : m_obj(const_cast<void *>(static_cast<const void *>(std::addressof(f)))),
          m_call(&callObject<std::remove_reference_t<F>>)
    {
    }

    /// Refers to a free function.
    constexpr FnRef(R (*fn)(Args...)) noexcept
        : m_fn(fn), m_call(&callFunction)
    {
    }

    R operator()(Args... args) const
    {
        return m_call(*this, std::forward<Args>(args)...);
    }

private:
    template <typename F>
    static R callObject(const FnRef &self, Args... args)
    {
        return std::invoke(*static_cast<F *>(self.m_obj), std::forward<Args>(args)...);
    }

    static R callFunction(const FnRef &self, Args... args)
    {
        return self.m_fn(std::forward<Args>(args)...);
    }

    union
    {
        void *m_obj;
        R (*m_fn)(Args...);
    };
    R (*m_call)(const FnRef &, Args...);
};

} // namespace dmm::lib
