// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tommi Tauriainen

#include "dmm/services/log.h"

#include <doctest/doctest.h>

#include <string>
#include <vector>

using namespace dmm::services;

namespace {

class CaptureSink final : public LogSink
{
public:
    void writeLine(std::string_view line) override { lines.emplace_back(line); }
    std::vector<std::string> lines;
};

} // namespace

TEST_CASE("log lines carry time, level and module")
{
    CaptureSink sink;
    log::drain(sink);
    sink.lines.clear();

    log::info("radio") << "tuned " << 145500 << " kHz";
    log::error("nand") << "bad block";
    CHECK(log::drain(sink) == 2);
    REQUIRE(sink.lines.size() == 2);
    CHECK(sink.lines[0].front() == '[');
    CHECK(sink.lines[0].find("] I radio: tuned 145500 kHz") != std::string::npos);
    CHECK(sink.lines[1].find("] E nand: bad block") != std::string::npos);
    CHECK(log::drain(sink) == 0);
}

TEST_CASE("a full log queue drops lines and says so")
{
    CaptureSink sink;
    log::drain(sink);
    sink.lines.clear();

    for (int i = 0; i < 200; ++i) {
        log::debug("flood") << "line " << i << " with some padding text";
    }
    const auto written = log::drain(sink);
    CHECK(written < 201);
    CHECK(sink.lines.back().find("lines dropped") != std::string::npos);
    CHECK(sink.lines.front().find("line 0 ") != std::string::npos);
}
