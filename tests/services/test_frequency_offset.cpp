// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Henri Lavikainen

#include "dmm/drivers/radio/rda1846.h"
#include "dmm/services/frequency_offset.h"

#include <doctest/doctest.h>

using dmm::drivers::radio::Rda1846;
using dmm::services::FmChannel;
using dmm::services::FrequencyOffset;

TEST_CASE("the offset steps by the tuning step, or a multiple of it of at least 10 Hz")
{
    CHECK(FrequencyOffset(62'500).stepMilliHz() == 62'500);
    CHECK(FrequencyOffset(62'500).maxSteps() == 80);
    CHECK(FrequencyOffset(1'000).stepMilliHz() == 10'000);
    CHECK(FrequencyOffset(1'000).maxSteps() == 500);
    CHECK(FrequencyOffset(3'000).stepMilliHz() == 12'000);
    CHECK(FrequencyOffset(0).stepMilliHz() == 10'000);
}

TEST_CASE("an offset in hertz takes the nearest step, within 5 kHz either way")
{
    FrequencyOffset offset(62'500);
    CHECK(offset.milliHz() == 0);
    offset.setHz(2000);
    CHECK(offset.steps() == 32);
    CHECK(offset.milliHz() == 2'000'000);
    offset.setHz(1950);
    CHECK(offset.milliHz() == 1'937'500);
    CHECK(offset.hz() == 1938);
    offset.setHz(-1950);
    CHECK(offset.milliHz() == -1'937'500);
    CHECK(offset.hz() == -1938);
    offset.setHz(31);
    CHECK(offset.steps() == 0);
    offset.setHz(32);
    CHECK(offset.steps() == 1);
    offset.setHz(-32);
    CHECK(offset.steps() == -1);
    offset.setHz(9000);
    CHECK(offset.hz() == 5000);
    offset.setHz(-9000);
    CHECK(offset.hz() == -5000);
    offset.setSteps(81);
    CHECK(offset.steps() == 80);
    offset.setSteps(-3);
    CHECK(offset.milliHz() == -187'500);
}

TEST_CASE("the kept whole hertz give the same step back")
{
    FrequencyOffset offset(62'500);
    FrequencyOffset again(62'500);
    for (int s = -offset.maxSteps(); s <= offset.maxSteps(); ++s) {
        offset.setSteps(s);
        again.setHz(offset.hz());
        CHECK(again.steps() == s);
    }
}

TEST_CASE("the offset moves both frequencies, and the chip tunes the exact step")
{
    FrequencyOffset offset(Rda1846::FrequencyStepMilliHz);
    offset.setSteps(-31);
    FmChannel channel;
    channel.receiveHz = 438'500'000;
    channel.transmitHz = 431'100'000;
    channel.wide = true;
    const FmChannel tuned = offset.apply(channel);
    CHECK(tuned.receiveHz == 438'498'062);
    CHECK(tuned.transmitHz == 431'098'062);
    CHECK(tuned.wide);
    // -1937.5 Hz is exactly 31 of the chip's steps.
    CHECK(Rda1846::frequencyWord(tuned.receiveHz) == 438'500u * 16 - 31);
    CHECK(Rda1846::frequencyWord(tuned.transmitHz) == 431'100u * 16 - 31);

    // No transmit frequency stays none.
    channel.transmitHz = 0;
    CHECK(offset.apply(channel).transmitHz == 0);
}

TEST_CASE("the offset reads as the value applied, to 0.1 Hz")
{
    FrequencyOffset offset(62'500);
    CHECK(offset.text().view() == "0 Hz");
    offset.setHz(2000);
    CHECK(offset.text().view() == "+2000 Hz");
    offset.setSteps(-31);
    CHECK(offset.text().view() == "-1937.5 Hz");
    offset.setSteps(1);
    CHECK(offset.text().view() == "+62.5 Hz");
    offset.setSteps(-80);
    CHECK(offset.text().view() == "-5000 Hz");
    FrequencyOffset fine(3'000);
    fine.setSteps(1);
    CHECK(fine.text().view() == "+12 Hz");
}
