// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tommi Tauriainen

#pragma once

#include <string_view>

namespace dmm::lib {

// std::string_view::starts_with() and ends_with() go through substr() and
// compare(), whose range checks call the library's exception helpers when
// the optimizer leaves them in (as at -O0). These never do.

constexpr bool startsWith(std::string_view text, std::string_view prefix)
{
    return text.size() >= prefix.size() && std::string_view(text.data(), prefix.size()) == prefix;
}

constexpr bool endsWith(std::string_view text, std::string_view suffix)
{
    return text.size() >= suffix.size() && std::string_view(text.data() + text.size() - suffix.size(), suffix.size()) == suffix;
}

} // namespace dmm::lib
