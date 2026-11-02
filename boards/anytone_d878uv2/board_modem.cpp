// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Henri Lavikainen

#include "dmm/board/board_modem.h"

#include "board_audio.h"
#include "pins.h"

#include "dmm/services/afsk.h"

namespace board {

namespace {

namespace hal = dmm::hal::gd32f30x;
using dmm::services::AfskModulator;
using dmm::services::RttyModulator;

static_assert(BoardModem::SampleRateHz == hal::PwmToneOutput::SampleRateHz);

AfskModulator modulator;
RttyModulator rtty;
// Set by stop(): the sample interrupt ends the sound.
volatile bool stopping = false;

int nextSample()
{
    return modulator.running() && !stopping ? modulator.next() : -1;
}

int nextRttySample()
{
    return rtty.running() && !stopping ? rtty.next() : -1;
}

std::uint8_t peakFor(std::uint8_t levelPercent)
{
    const unsigned level = levelPercent > 100 ? 100 : levelPercent;
    return static_cast<std::uint8_t>(level * 255 / 100);
}

} // namespace

void BoardModem::play(int (*source)())
{
    stopping = false;
    BoardAudio::s_packet = true;
    hal::configure(ToneOutput, hal::PinMode::Alternate, hal::PinSpeed::Medium);
    BoardAudio::s_tone.playSamples(source);
}

bool BoardModem::send(std::span<const std::uint8_t> frame, std::uint16_t leadMs, std::uint8_t levelPercent)
{
    if (BoardAudio::s_packet) {
        return false;
    }
    // A flag is eight bits.
    const auto leadFlags = static_cast<std::uint16_t>((leadMs * AfskModulator::BaudRate / 8 + 999) / 1000);
    modulator.start(frame, leadFlags == 0 ? 1 : leadFlags, PacketTailFlags, SampleRateHz, peakFor(levelPercent));
    play(&nextSample);
    return true;
}

bool BoardModem::sendRtty(std::span<const std::uint8_t> codes, const RttyModulator::Config &config, std::uint16_t leadMs,
    std::uint16_t tailMs, std::uint8_t levelPercent)
{
    if (BoardAudio::s_packet) {
        return false;
    }
    rtty.start(codes, config, leadMs, tailMs, SampleRateHz, peakFor(levelPercent));
    play(&nextRttySample);
    return true;
}

void BoardModem::stop()
{
    if (BoardAudio::s_packet) {
        stopping = true;
    }
}

bool BoardModem::sending()
{
    if (BoardAudio::s_packet && !BoardAudio::s_tone.playing()) {
        BoardAudio::s_packet = false;
        hal::configure(ToneOutput, hal::PinMode::Analog);
    }
    return BoardAudio::s_packet;
}

} // namespace board
