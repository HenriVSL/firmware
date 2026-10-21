// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tommi Tauriainen

#include "dmm/hal/gd32f30x/watchdog.h"

#include "dmm/hal/gd32f30x/registers.h"

namespace dmm::hal::gd32f30x {

namespace {

constexpr std::uint32_t RcHz = 40'000;
constexpr std::uint32_t PrescalerDiv32 = 3;
constexpr std::uint32_t MaxReload = 0xFFF;

} // namespace

void startWatchdog(std::uint32_t timeoutMs)
{
    std::uint32_t reload = timeoutMs * (RcHz / 32) / 1000;
    if (reload > MaxReload) {
        reload = MaxReload;
    }
    reg(fwdgt::Ctl) = fwdgt::KeyUnlock;
    while ((reg(fwdgt::Stat) & fwdgt::PrescalerBusy) != 0) {
    }
    reg(fwdgt::Psc) = PrescalerDiv32;
    while ((reg(fwdgt::Stat) & fwdgt::ReloadBusy) != 0) {
    }
    reg(fwdgt::Rld) = reload;
    reg(fwdgt::Ctl) = fwdgt::KeyReload;
    reg(fwdgt::Ctl) = fwdgt::KeyStart;
}

void feedWatchdog()
{
    reg(fwdgt::Ctl) = fwdgt::KeyReload;
}

} // namespace dmm::hal::gd32f30x
