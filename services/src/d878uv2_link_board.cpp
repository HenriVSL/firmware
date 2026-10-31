// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Henri Lavikainen

#include "dmm/services/d878uv2_link_board.h"

#include "dmm/lib/strings.h"

namespace dmm::services {

namespace {

constexpr std::string_view PacketHeader = "+APRS:";
constexpr std::uint8_t Flag = 0x7E;
// Two addresses, control, protocol and the check.
constexpr std::size_t MinFrame = 7 * 2 + 2 + 2;

bool printable(std::uint8_t byte)
{
    return byte >= ' ' && byte <= '~';
}

char upper(std::uint8_t byte)
{
    return static_cast<char>(byte >= 'a' && byte <= 'z' ? byte - 'a' + 'A' : byte);
}

// True if @p bytes hold @p word, in any case.
bool holds(std::span<const std::uint8_t> bytes, std::string_view word)
{
    for (std::size_t at = 0; at + word.size() <= bytes.size(); ++at) {
        std::size_t i = 0;
        while (i < word.size() && upper(bytes[at + i]) == word[i]) {
            ++i;
        }
        if (i == word.size()) {
            return true;
        }
    }
    return false;
}

int hexDigit(char c)
{
    return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10
        : c >= 'A' && c <= 'F'                                   ? c - 'A' + 10
                                                                 : -1;
}

} // namespace

D878Uv2LinkBoardReader::CommandResult D878Uv2LinkBoardReader::command(std::string_view text,
    std::span<std::uint8_t, MaxCommand> out, std::size_t &length)
{
    length = 0;
    constexpr std::string_view HexPrefix = "hex ";
    if (lib::startsWith(text, HexPrefix) || text == "hex") {
        text.remove_prefix(text.size() < HexPrefix.size() ? text.size() : HexPrefix.size());
        int high = -1;
        for (char c : text) {
            if (c == ' ') {
                high = -1;
                continue;
            }
            const int digit = hexDigit(c);
            if (digit < 0) {
                return CommandResult::Invalid;
            }
            if (high < 0) {
                if (length == out.size()) {
                    return CommandResult::Invalid;
                }
                out[length++] = static_cast<std::uint8_t>(digit);
                high = digit;
            } else if (high < 0x10) {
                // A second digit; a third makes it no byte.
                out[length - 1] = static_cast<std::uint8_t>(high << 4 | digit);
                high = 0x10;
            } else {
                return CommandResult::Invalid;
            }
        }
    } else {
        if (text.empty() || text.size() + 2 > out.size()) {
            return CommandResult::Invalid;
        }
        for (char c : text) {
            out[length++] = static_cast<std::uint8_t>(c);
        }
        out[length++] = '\r';
        out[length++] = '\n';
    }
    if (length == 0) {
        return CommandResult::Invalid;
    }
    const std::span<const std::uint8_t> bytes(out.data(), length);
    if (holds(bytes, "UPDATE") || holds(bytes, "PROGRAM")) {
        length = 0;
        return CommandResult::Refused;
    }
    return CommandResult::Ok;
}

D878Uv2LinkBoardReader::Result D878Uv2LinkBoardReader::reset()
{
    const bool inFrame = m_state == State::Frame && m_frameLength > 0;
    m_state = State::Idle;
    return inFrame ? failFrame() : Result::None;
}

D878Uv2LinkBoardReader::Result D878Uv2LinkBoardReader::failFrame()
{
    m_bad = {};
    if (m_lastFlag != NoFlag) {
        const std::size_t body = m_lastFlag - 2;
        m_bad.length = m_lastFlag;
        m_bad.closed = true;
        m_bad.receivedCheck = static_cast<std::uint16_t>(m_frame[body] | m_frame[body + 1] << 8);
        m_bad.computedCheck = ax25::frameCheck({m_frame.data(), body});
    } else {
        m_bad.length = m_frameLength;
    }
    ++m_badCount;
    return Result::BadFrame;
}

D878Uv2LinkBoardReader::Result D878Uv2LinkBoardReader::feed(std::uint8_t byte)
{
    switch (m_state) {
    case State::Idle:
        if (byte == '+') {
            m_line[0] = '+';
            m_lineLength = 1;
            m_state = State::Header;
        } else if (printable(byte)) {
            m_line[0] = static_cast<char>(byte);
            m_lineLength = 1;
            m_state = State::Text;
        } else if (byte != '\r' && byte != '\n') {
            m_byte = byte;
            return Result::Byte;
        }
        return Result::None;
    case State::Text:
        // A '+' starts a message of its own; a control byte spoils the line.
        if (byte == '+') {
            m_line[0] = '+';
            m_lineLength = 1;
            m_state = State::Header;
            return Result::None;
        }
        if (byte == '\n') {
            m_state = State::Idle;
            return Result::Line;
        }
        if (byte == '\r') {
            return Result::None;
        }
        if (!printable(byte) || m_lineLength == m_line.size()) {
            m_state = State::Idle;
            return Result::None;
        }
        m_line[m_lineLength++] = static_cast<char>(byte);
        return Result::None;
    case State::Header:
        if (byte == '\n') {
            if (m_lineLength > 0 && m_line[m_lineLength - 1] == '\r') {
                --m_lineLength;
            }
            m_state = State::Idle;
            return Result::Line;
        }
        if (m_lineLength == m_line.size()) {
            m_state = State::Idle;
            return Result::None;
        }
        m_line[m_lineLength++] = static_cast<char>(byte);
        if (line() == PacketHeader) {
            m_state = State::FrameStart;
        }
        return Result::None;
    case State::FrameStart:
        m_frameLength = 0;
        m_lastFlag = NoFlag;
        m_state = byte == Flag ? State::Frame : State::Idle;
        return Result::None;
    case State::Frame:
        if (byte == Flag && m_frameLength >= MinFrame) {
            const std::size_t body = m_frameLength - 2;
            const auto check = static_cast<std::uint16_t>(m_frame[body] | m_frame[body + 1] << 8);
            if (ax25::frameCheck({m_frame.data(), body}) == check) {
                m_state = State::Idle;
                ++m_goodCount;
                return Result::Frame;
            }
            m_lastFlag = m_frameLength;
        }
        if (m_frameLength == m_frame.size()) {
            m_state = State::Idle;
            return failFrame();
        }
        m_frame[m_frameLength++] = byte;
        // The next packet's header right after a flag ends a failed frame.
        if (m_lastFlag != NoFlag && m_frameLength == m_lastFlag + 1 + PacketHeader.size() &&
            std::string_view(reinterpret_cast<const char *>(&m_frame[m_lastFlag + 1]), PacketHeader.size()) == PacketHeader) {
            m_state = State::FrameStart;
            return failFrame();
        }
        return Result::None;
    }
    return Result::None;
}

} // namespace dmm::services
