// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tommi Tauriainen

#pragma once

#include "dmm/lib/fn_ref.h"

#include <cstdint>
#include <span>

namespace dmm::services {

struct BasebandInfo
{
    bool running = false;
    /// Why it is not running, for the log; nullptr while running.
    const char *problem = nullptr;
    char product[16] = {};
    char hardware[16] = {};
    char software[16] = {};
    /// The chip's "data waiting" line, read when info() is called.
    bool dataWaiting = false;
};

/// The radio's baseband processor and the audio path behind it (codec,
/// speaker). Used by the radio task only.
class Baseband
{
public:
    /// Powers the chip up, loads it and sets up the analog audio path.
    /// Takes a few seconds.
    virtual bool start() = 0;
    virtual BasebandInfo info() = 0;

    /// Speaker volume: 0 = muted, 1..VolumeSteps = quietest..loudest.
    static constexpr int VolumeSteps = 15;
    virtual bool setSpeakerVolume(int step) = 0;

    /// Plays a tone of @p hz for @p ms on the speaker path; @p amplitude
    /// 0..64 is 0..1 of full scale.
    virtual bool playTone(std::uint16_t hz, std::uint16_t ms, std::uint8_t amplitude) = 0;

    /// Analog FM: the chip filters and levels the audio in both directions,
    /// with pre- and de-emphasis, for a @p wide or narrow channel, at the
    /// radio's calibrated levels. False without a calibration.
    virtual bool setAnalog(bool wide) = 0;

    enum class WorkMode : std::uint8_t
    {
        Idle = 0,
        Receive = 1,
        Transmit = 2,
    };
    virtual bool setWorkMode(WorkMode mode) = 0;

    /// Tells the chip a carrier is present (true) or gone; in analog mode
    /// it passes receive audio only while one is.
    virtual bool setCarrier(bool present) = 0;

    /// Connects the microphone for transmit (true) or disconnects it and
    /// restores the receive audio levels.
    virtual bool setMicrophone(bool on) = 0;

    /// Writes one audio codec register (page, register, value), for
    /// experiments from the console.
    virtual bool codecWrite(std::uint8_t page, std::uint8_t reg, std::uint8_t value) = 0;

    /// Sends one raw packet of @p type with @p fields, then hands every
    /// packet the chip sends within @p waitMs (at most four) to @p onPacket.
    /// Works whether or not start() succeeded.
    virtual void exchange(std::uint8_t type, std::span<const std::uint8_t> fields, unsigned waitMs,
        lib::FnRef<void(std::uint8_t type, std::span<const std::uint8_t> fields)> onPacket) = 0;

protected:
    ~Baseband() = default;
};

/// What a board does around the baseband chip: power and reset.
class BasebandBoard
{
public:
    /// Powers the chip and resets it, leaving it waiting for its boot
    /// program. Blocks for about a second.
    virtual void powerUpAndReset() = 0;

protected:
    ~BasebandBoard() = default;
};

} // namespace dmm::services
