// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tommi Tauriainen

#pragma once

#include "dmm/drivers/baseband/sct3258.h"
#include "dmm/services/baseband.h"

#include <span>

namespace dmm::services {

/// The SCT3258 with a TLV320AIC3204 audio codec behind it. The codec is set
/// up by the SCT3258 once told which codec it has; the speaker volume is
/// the codec's headphone-left driver gain (page 1, register 16).
class Sct3258Baseband final : public Baseband
{
public:
    Sct3258Baseband(drivers::baseband::Sct3258 &chip, BasebandBoard &board, std::span<const std::uint8_t> bootBlob);

    bool start() override;
    BasebandInfo info() override;
    bool setSpeakerVolume(int step) override;
    bool playTone(std::uint16_t hz, std::uint16_t ms, std::uint8_t amplitude) override;
    bool setAnalog(bool wide, std::span<const std::uint8_t, 10> gains) override;
    bool setWorkMode(WorkMode mode) override;
    bool setCarrier(bool present) override;
    bool setMicrophone(bool on) override;
    bool setCodecTrims(std::uint8_t leftOut, std::uint8_t rightOut, std::uint8_t rightInput) override;
    bool codecWrite(std::uint8_t page, std::uint8_t reg, std::uint8_t value) override;
    void exchange(std::uint8_t type, std::span<const std::uint8_t> fields, unsigned waitMs,
        lib::FnRef<void(std::uint8_t type, std::span<const std::uint8_t> fields)> onPacket) override;

    /// The driver-gain register value for a volume step: 0x40 (muted) for
    /// 0, else -6 dB at step 1 rising to +23 dB at the top step, as a 6-bit
    /// two's-complement dB value.
    static std::uint8_t volumeRegister(int step);

private:
    bool fail(const char *problem);

    drivers::baseband::Sct3258 &m_chip;
    BasebandBoard &m_board;
    std::span<const std::uint8_t> m_blob;
    BasebandInfo m_info;
};

} // namespace dmm::services
