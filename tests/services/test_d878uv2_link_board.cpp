// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Henri Lavikainen

#include "dmm/services/ax25.h"
#include "dmm/services/d878uv2_link_board.h"

#include <doctest/doctest.h>

#include <array>
#include <string>
#include <vector>

using namespace dmm::services;
using Result = D878Uv2LinkBoardReader::Result;

namespace {

std::vector<std::uint8_t> encodedFrame(std::string_view info)
{
    ax25::Frame f;
    f.source = *ax25::parseAddress("N0CALL-7");
    f.destination = *ax25::parseAddress("APZDMM");
    f.path.pushBack(*ax25::parseAddress("WIDE1-1"));
    f.info << info;
    std::array<std::uint8_t, ax25::MaxFrameBytes> bytes{};
    const std::size_t n = ax25::encode(f, bytes);
    return {bytes.begin(), bytes.begin() + static_cast<std::ptrdiff_t>(n)};
}

struct Fed
{
    std::vector<std::vector<std::uint8_t>> frames;
    std::vector<std::string> lines;
};

Fed feedAll(D878Uv2LinkBoardReader &r, const std::vector<std::uint8_t> &bytes)
{
    Fed out;
    for (std::uint8_t b : bytes) {
        switch (r.feed(b)) {
        case Result::Frame:
            out.frames.emplace_back(r.frame().begin(), r.frame().end());
            break;
        case Result::Line:
            out.lines.emplace_back(r.line());
            break;
        case Result::Byte:
        case Result::None:
            break;
        }
    }
    return out;
}

void append(std::vector<std::uint8_t> &to, std::string_view text)
{
    to.insert(to.end(), text.begin(), text.end());
}

} // namespace

TEST_CASE("link board: the start answer, then packets between flags")
{
    const auto first = encodedFrame("!6010.25N/02456.30E>Test");
    const auto second = encodedFrame(">status ~ with flags ~~ inside");
    std::vector<std::uint8_t> stream{0x06};
    append(stream, "+READY\r\n+APRS:~");
    stream.insert(stream.end(), first.begin(), first.end());
    append(stream, "~+APRS:~");
    stream.insert(stream.end(), second.begin(), second.end());
    stream.push_back(0x7E);

    D878Uv2LinkBoardReader r;
    const Fed fed = feedAll(r, stream);
    REQUIRE(fed.lines.size() == 1);
    CHECK(fed.lines[0] == "+READY");
    REQUIRE(fed.frames.size() == 2);
    CHECK(fed.frames[0] == first);
    CHECK(fed.frames[1] == second);
    ax25::Frame f;
    REQUIRE(ax25::decode(fed.frames[1], f));
    CHECK(f.info.view() == ">status ~ with flags ~~ inside");
}

TEST_CASE("link board: a damaged frame is dropped and the next one still read")
{
    auto bad = encodedFrame("!6010.25N/02456.30E>Test");
    bad[20] ^= 0x40;
    const auto good = encodedFrame(">ok");
    std::vector<std::uint8_t> stream;
    append(stream, "+APRS:~");
    stream.insert(stream.end(), bad.begin(), bad.end());
    stream.push_back(0x7E);

    D878Uv2LinkBoardReader r;
    CHECK(feedAll(r, stream).frames.empty());
    // The board gives no further sign; a pause in the data resets the reader.
    r.reset();
    std::vector<std::uint8_t> next;
    append(next, "+APRS:~");
    next.insert(next.end(), good.begin(), good.end());
    next.push_back(0x7E);
    const Fed fed = feedAll(r, next);
    REQUIRE(fed.frames.size() == 1);
    CHECK(fed.frames[0] == good);
}

TEST_CASE("link board: other text lines and control bytes")
{
    std::vector<std::uint8_t> stream{0x06};
    append(stream, "OK\r\nnoise");
    stream.push_back(0x01); // spoils the line
    append(stream, "x+VER:1\r\n");

    D878Uv2LinkBoardReader r;
    std::vector<std::string> lines;
    std::vector<std::uint8_t> bytes;
    for (std::uint8_t b : stream) {
        const Result result = r.feed(b);
        if (result == Result::Line) {
            lines.emplace_back(r.line());
        } else if (result == Result::Byte) {
            bytes.push_back(r.byte());
        }
    }
    REQUIRE(lines.size() == 2);
    CHECK(lines[0] == "OK");
    CHECK(lines[1] == "+VER:1");
    CHECK(bytes == std::vector<std::uint8_t>{0x06});
}

TEST_CASE("link board: console commands")
{
    using CommandResult = D878Uv2LinkBoardReader::CommandResult;
    std::array<std::uint8_t, D878Uv2LinkBoardReader::MaxCommand> out{};
    std::size_t n = 0;
    auto bytes = [&] { return std::vector<std::uint8_t>(out.begin(), out.begin() + static_cast<std::ptrdiff_t>(n)); };

    CHECK(D878Uv2LinkBoardReader::command("AT+VER?", out, n) == CommandResult::Ok);
    CHECK(bytes() == std::vector<std::uint8_t>{'A', 'T', '+', 'V', 'E', 'R', '?', '\r', '\n'});
    CHECK(D878Uv2LinkBoardReader::command("hex EE 01 6", out, n) == CommandResult::Ok);
    CHECK(bytes() == std::vector<std::uint8_t>{0xEE, 0x01, 0x06});
    CHECK(D878Uv2LinkBoardReader::command("hex ee  0a", out, n) == CommandResult::Ok);
    CHECK(bytes() == std::vector<std::uint8_t>{0xEE, 0x0A});

    CHECK(D878Uv2LinkBoardReader::command("UPDATE", out, n) == CommandResult::Refused);
    CHECK(n == 0);
    CHECK(D878Uv2LinkBoardReader::command("program now", out, n) == CommandResult::Refused);
    CHECK(D878Uv2LinkBoardReader::command("AT+Update", out, n) == CommandResult::Refused);
    // "PROGRAM" in hex.
    CHECK(D878Uv2LinkBoardReader::command("hex 50 52 4f 47 52 41 4d", out, n) == CommandResult::Refused);
    CHECK(D878Uv2LinkBoardReader::command("hex 01 75 70 64 61 74 65 02", out, n) == CommandResult::Refused);

    CHECK(D878Uv2LinkBoardReader::command("", out, n) == CommandResult::Invalid);
    CHECK(D878Uv2LinkBoardReader::command("hex 123", out, n) == CommandResult::Invalid);
    CHECK(D878Uv2LinkBoardReader::command("hex zz", out, n) == CommandResult::Invalid);
    CHECK(D878Uv2LinkBoardReader::command("hex ", out, n) == CommandResult::Invalid);
    CHECK(D878Uv2LinkBoardReader::command("hex", out, n) == CommandResult::Invalid);
    CHECK(D878Uv2LinkBoardReader::command(std::string(80, 'A'), out, n) == CommandResult::Invalid);
}
