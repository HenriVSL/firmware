// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tommi Tauriainen

#pragma once

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <span>

namespace dmm::services {

/// A byte stream to a PC, such as a USB serial port; the board names the
/// type (dmm/board/parts.h). Used by one task.
///
/// - isOpen(): a program on the other side is listening.
/// - write(data): queues what fits of @p data; returns how much.
/// - read(data): takes what has arrived, up to @p data's size.
template <class T>
concept ByteChannel = requires(T &channel, std::span<const std::uint8_t> out, std::span<std::uint8_t> in) {
    { channel.isOpen() } -> std::same_as<bool>;
    { channel.write(out) } -> std::same_as<std::size_t>;
    { channel.read(in) } -> std::same_as<std::size_t>;
};

} // namespace dmm::services
