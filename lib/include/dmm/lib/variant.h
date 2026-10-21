// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tommi Tauriainen

#pragma once

#include <cstddef>
#include <type_traits>
#include <variant>

namespace dmm::lib {

/// Builds one visitor out of several lambdas.
template <typename... F>
struct Overload : F...
{
    using F::operator()...;
};

namespace detail {

template <std::size_t I, typename V, typename F>
decltype(auto) visitFrom(F &&visitor, V &variant)
{
    constexpr std::size_t Last = std::variant_size_v<std::remove_const_t<V>> - 1;
    if constexpr (I == Last) {
        return visitor(*std::get_if<I>(&variant));
    } else {
        if (variant.index() == I) {
            return visitor(*std::get_if<I>(&variant));
        }
        return visitFrom<I + 1>(static_cast<F &&>(visitor), variant);
    }
}

} // namespace detail

/// std::visit without the valueless-variant check, which calls abort() in
/// unoptimised builds without exceptions. Variants here never go
/// valueless: nothing throws.
template <typename F, typename... T>
decltype(auto) visit(F &&visitor, std::variant<T...> &variant)
{
    return detail::visitFrom<0>(static_cast<F &&>(visitor), variant);
}

template <typename F, typename... T>
decltype(auto) visit(F &&visitor, const std::variant<T...> &variant)
{
    return detail::visitFrom<0>(static_cast<F &&>(visitor), variant);
}

} // namespace dmm::lib
