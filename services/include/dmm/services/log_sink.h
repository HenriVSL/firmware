// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tommi Tauriainen

#pragma once

#include <cstddef>
#include <span>
#include <string_view>

namespace dmm::services {

/// Destination for finished log lines: a USB serial port, a console, a window.
class LogSink
{
public:
    /// Receives one line without its trailing newline.
    virtual void writeLine(std::string_view line) = 0;

    /// False while nobody is listening; lines then wait in the log queue.
    virtual bool ready() { return true; }

    /// Copies typed input from the same channel, if it has any, and returns
    /// how many bytes were copied.
    virtual std::size_t readInput(std::span<char> buffer)
    {
        (void)buffer;
        return 0;
    }

protected:
    ~LogSink() = default;
};

} // namespace dmm::services
