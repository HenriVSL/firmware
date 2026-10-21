// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tommi Tauriainen

#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

namespace dmm::services {

/// A byte stream to a PC, such as a USB serial port. Used by one task.
class ByteChannel
{
public:
    /// A program on the other side is listening.
    virtual bool isOpen() = 0;
    /// Queues what fits of @p data; returns how much.
    virtual std::size_t write(std::span<const std::uint8_t> data) = 0;
    virtual std::size_t read(std::span<std::uint8_t> data) = 0;

protected:
    ~ByteChannel() = default;
};

} // namespace dmm::services
