// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Henri Lavikainen

#pragma once

#include "dmm/services/ax25.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace dmm::services {

/// What the D878UVII's BT+APRS board sends the radio over its serial link
/// (115200 baud, 8N1), as observed. Text messages are lines starting with
/// '+' and ending in CR LF, such as "+READY" once the board is up; other
/// printable lines are passed on too, and stray control bytes (such as its
/// 0x06 answer to the start command) one by one. A packet
/// it decoded comes as "+APRS:", a flag (0x7E), the AX.25 frame from the
/// first address to the frame check sequence, and a closing flag. The
/// frame itself may hold 0x7E bytes, so a flag ends it only where the frame
/// check before it is right. A frame whose check never comes right ends at
/// the next "+APRS:" after a flag, when the buffer is full, or at reset();
/// it is counted as failed and described by badFrame().
class D878Uv2LinkBoardReader
{
public:
    /// Sent three times, 50 ms apart, after the board is powered: it then
    /// answers 0x06 and "+READY".
    static constexpr std::array<std::uint8_t, 3> StartCommand{0xEE, 0x01, 0x06};
    static constexpr std::size_t MaxLine = 64;
    /// Longest console command for the board, in bytes.
    static constexpr std::size_t MaxCommand = 72;

    enum class CommandResult : std::uint8_t
    {
        Ok,
        Refused, // would enter the board's update or programming mode
        Invalid,
    };

    /// Bytes for the board from console text: a text line, sent with CR LF
    /// added, or "hex <bytes>" (bytes in hex, separated by spaces) sent as
    /// they are. Anything holding "UPDATE" or "PROGRAM" in any case is
    /// refused: those words can make the board's boot loader erase its
    /// program. @p length gets the byte count.
    static CommandResult command(std::string_view text, std::span<std::uint8_t, MaxCommand> out, std::size_t &length);

    enum class Result : std::uint8_t
    {
        None,     // the byte did not end anything
        Frame,    // frame() holds a packet whose frame check is right
        Line,     // line() holds a text message
        Byte,     // byte() is a control byte outside any message
        BadFrame, // a frame whose check failed ended; see badFrame()
    };

    /// A frame whose check failed.
    struct BadFrame
    {
        /// Bytes up to the last flag that could have closed it (frame and
        /// check), or all bytes kept when none came.
        std::size_t length = 0;
        /// False when no closing flag came: then no check was found.
        bool closed = false;
        std::uint16_t receivedCheck = 0;
        std::uint16_t computedCheck = 0;
    };

    /// Takes one byte; says whether it ended a frame or a line.
    Result feed(std::uint8_t byte);
    /// Drops a partial message, for example after a pause in the data.
    /// Returns BadFrame if that was a frame.
    Result reset();

    /// The last frame, valid until the next feed().
    std::span<const std::uint8_t> frame() const { return {m_frame.data(), m_frameLength}; }
    /// The last line without CR LF, valid until the next feed().
    std::string_view line() const { return {m_line.data(), m_lineLength}; }
    std::uint8_t byte() const { return m_byte; }
    /// The last frame that failed its check.
    const BadFrame &badFrame() const { return m_bad; }
    /// Frames whose check was right and frames whose check failed, since
    /// construction.
    std::uint32_t goodFrames() const { return m_goodCount; }
    std::uint32_t badFrames() const { return m_badCount; }

private:
    static constexpr std::size_t NoFlag = static_cast<std::size_t>(-1);

    Result failFrame();

    enum class State : std::uint8_t
    {
        Idle,
        Header,
        Text,
        FrameStart,
        Frame,
    };

    State m_state = State::Idle;
    std::array<char, MaxLine> m_line{};
    std::size_t m_lineLength = 0;
    std::uint8_t m_byte = 0;
    std::array<std::uint8_t, ax25::MaxFrameBytes> m_frame{};
    std::size_t m_frameLength = 0;
    // Where a flag whose check failed was kept in m_frame, or NoFlag.
    std::size_t m_lastFlag = NoFlag;
    BadFrame m_bad;
    std::uint32_t m_goodCount = 0;
    std::uint32_t m_badCount = 0;
};

} // namespace dmm::services
