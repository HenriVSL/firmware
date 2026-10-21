// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tommi Tauriainen

#pragma once

#include "dmm/services/audio.h"
#include "dmm/services/log.h"

/// The speaker as log lines: the simulator has no sound output.
class SimAudio final : public dmm::services::AudioHardware
{
public:
    void setAmplifier(bool on) override { dmm::services::log::info("sim") << "speaker " << (on ? "on" : "off"); }
    void setReceiveRoute(bool on) override { dmm::services::log::info("sim") << "receive audio " << (on ? "on" : "off"); }
    void startTone(std::uint16_t hz, int level) override
    {
        dmm::services::log::info("sim") << "tone " << hz << " Hz, level " << level;
    }
    void stopTone() override {}
    void setToneOutput(bool) override {}
    unsigned amplifierSettleMs() const override { return 0; }
};
