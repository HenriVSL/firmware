// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tommi Tauriainen

#include "dmm/services/sct3258_baseband.h"

#include "dmm/services/log.h"

namespace dmm::services {

namespace {

using drivers::baseband::Sct3258;
namespace log = services::log;

constexpr std::uint8_t CodecTlv320aic3204 = 1;
constexpr std::uint8_t VocoderAmbePlus2 = 0x04;
constexpr std::uint8_t VocoderAnalog = 0x10;
constexpr std::uint8_t ProcessAnalog = 0x80;

// Analog mode byte: DCS high-pass on and 1.5 dB less at 3 kHz; the chip's
// own tone generator, the compander and the emphasis skip stay off. Wide
// channels add the 3 kHz audio low-pass (else 2.55 kHz).
constexpr std::uint8_t AnalogNarrow = 0xC0;
constexpr std::uint8_t AnalogWide = AnalogNarrow | 0x02;

// Reply times from the packet interface document, with margin.
constexpr unsigned CodecSelectMs = 1500;
constexpr unsigned VocoderSelectMs = 3500;

// TLV320AIC3204 registers (page, register).
constexpr std::uint8_t HplPage = 1;
constexpr std::uint8_t HplRegister = 16; // headphone-left driver gain, 6-bit dB
constexpr std::uint8_t HplMuted = 0x40;
constexpr std::uint8_t DacVolumePage = 0;
constexpr std::uint8_t LeftDacVolume = 65; // signed, 0.5 dB steps
constexpr std::uint8_t MicRoutingPage = 1;
constexpr std::uint8_t LeftMicPositiveRouting = 52; // 0: microphone not connected

// The speaker is loud: the DAC is turned down by 22 dB, and the driver gain
// then spans -6 to +23 dB over the volume steps, more finely at the bottom.
constexpr std::uint8_t DacVolumeMinus22Db = static_cast<std::uint8_t>(-44);
constexpr std::uint8_t DacVolume0Db = 0;

// Transmit microphone path: left input through 20 kOhm, input amplifier
// +16 dB, left AGC on with the same target, as the board is set up for.
constexpr std::uint8_t MicIn1Left20k = 0x80;
constexpr std::uint8_t LeftMicPgaVolume = 59;
constexpr std::uint8_t MicGain16Db = 32;
constexpr std::uint8_t AgcPage = 0;
constexpr std::uint8_t LeftAgcControl = 86;
constexpr std::uint8_t LeftAgcMaxGain = 88;
constexpr std::uint8_t AgcOn = 0x80;

// Calibrated trims: line-out drivers and the right input amplifier.
constexpr std::uint8_t TrimPage = 1;
constexpr std::uint8_t LeftOutGain = 18;
constexpr std::uint8_t RightOutGain = 19;
constexpr std::uint8_t RightMicPgaVolume = 60;
constexpr std::int8_t StepDb[Baseband::VolumeSteps] = {-6, -4, -1, 2, 6, 8, 10, 12, 13, 15, 16, 18, 20, 22, 23};

const char *bootProblem(Sct3258::BootResult r)
{
    switch (r) {
    case Sct3258::BootResult::Ready:
        return nullptr;
    case Sct3258::BootResult::NoFirmware:
        return "booted, but reports no firmware";
    case Sct3258::BootResult::NoAnswer:
        return "no answer after the boot program";
    case Sct3258::BootResult::WrongAnswer:
        return "unexpected answer after the boot program";
    case Sct3258::BootResult::BadBlob:
        return "boot program has the wrong size";
    }
    return "?";
}

} // namespace

Sct3258Baseband::Sct3258Baseband(Sct3258 &chip, BasebandBoard &board, std::span<const std::uint8_t> bootBlob)
    : m_chip(chip), m_board(board), m_blob(bootBlob)
{
}

bool Sct3258Baseband::fail(const char *problem)
{
    m_info.running = false;
    m_info.problem = problem;
    return false;
}

bool Sct3258Baseband::start()
{
    m_info = {};
    if (m_blob.empty()) {
        return fail("this build has no boot program (configure with DMM_STOCK_FIRMWARE)");
    }
    m_board.powerUpAndReset();
    std::uint8_t ack = 0xFF;
    const Sct3258::BootResult boot = m_chip.boot(m_blob, ack);
    if (boot != Sct3258::BootResult::Ready) {
        log::error("baseband") << bootProblem(boot) << " (code " << ack << ")";
        return fail(bootProblem(boot));
    }
    m_chip.queryText(Sct3258::QueryProduct, m_info.product, sizeof m_info.product);
    m_chip.queryText(Sct3258::QueryHardware, m_info.hardware, sizeof m_info.hardware);
    m_chip.queryText(Sct3258::QuerySoftware, m_info.software, sizeof m_info.software);

    const std::uint8_t codec[] = {Sct3258::CodecSelect, CodecTlv320aic3204};
    const int codecStatus = m_chip.control(codec, CodecSelectMs);
    if (codecStatus != 0) {
        log::warning("baseband") << "codec selection answered " << codecStatus;
    }
    // The vocoder is loaded once, then the chip is put in analog mode.
    for (std::uint8_t vocoder : {VocoderAmbePlus2, VocoderAnalog}) {
        const std::uint8_t select[] = {Sct3258::VocoderSelect, vocoder};
        const int status = m_chip.control(select, VocoderSelectMs);
        if (status != 0) {
            log::warning("baseband") << "vocoder " << vocoder << " selection answered " << status;
        }
    }
    m_info.running = true;
    // Receive level, microphone disconnected.
    const bool audioSet = m_chip.codecWrite(DacVolumePage, LeftDacVolume, DacVolumeMinus22Db) && m_chip.codecWrite(MicRoutingPage, LeftMicPositiveRouting, 0x00);
    if (!audioSet) {
        log::warning("baseband") << "codec did not take the audio set-up";
    }
    return true;
}

BasebandInfo Sct3258Baseband::info()
{
    BasebandInfo i = m_info;
    i.dataWaiting = m_chip.dataWaiting();
    return i;
}

std::uint8_t Sct3258Baseband::volumeRegister(int step)
{
    if (step <= 0) {
        return HplMuted;
    }
    if (step > VolumeSteps) {
        step = VolumeSteps;
    }
    return static_cast<std::uint8_t>(StepDb[step - 1] & 0x3F);
}

bool Sct3258Baseband::setSpeakerVolume(int step)
{
    if (!m_info.running) {
        return false;
    }
    return m_chip.codecWrite(HplPage, HplRegister, volumeRegister(step));
}

bool Sct3258Baseband::playTone(std::uint16_t hz, std::uint16_t ms, std::uint8_t amplitude)
{
    if (!m_info.running) {
        return false;
    }
    // One repetition of tone 1 to the local line out, tone 2 silent.
    const std::uint8_t tone[] = {Sct3258::AnalogTone, 0x01, static_cast<std::uint8_t>(hz >> 8), static_cast<std::uint8_t>(hz), 0,
        0, 0, 0, 0, 0, amplitude, 0, 0, 0, static_cast<std::uint8_t>(ms >> 8), static_cast<std::uint8_t>(ms), 0, 1};
    return m_chip.control(tone) == 0;
}

bool Sct3258Baseband::setAnalog(bool wide, std::span<const std::uint8_t, 10> gains)
{
    if (!m_info.running) {
        return false;
    }
    const std::uint8_t process[] = {Sct3258::ProcessMode, ProcessAnalog};
    std::uint8_t config[2 + 10] = {Sct3258::AnalogConfig, wide ? AnalogWide : AnalogNarrow};
    for (std::size_t i = 0; i < gains.size(); ++i) {
        config[2 + i] = gains[i];
    }
    return m_chip.control(process) == 0 && m_chip.control(config) == 0;
}

bool Sct3258Baseband::setWorkMode(WorkMode mode)
{
    const std::uint8_t work[] = {Sct3258::WorkMode, static_cast<std::uint8_t>(mode), 0, 0};
    return m_info.running && m_chip.control(work) == 0;
}

bool Sct3258Baseband::setCarrier(bool present)
{
    const std::uint8_t carrier[] = {Sct3258::CarrierReady, static_cast<std::uint8_t>(present ? 1 : 0)};
    return m_info.running && m_chip.control(carrier) == 0;
}

bool Sct3258Baseband::setMicrophone(bool on)
{
    if (!m_info.running) {
        return false;
    }
    if (!on) {
        return m_chip.codecWrite(MicRoutingPage, LeftMicPositiveRouting, 0x00) &&
            m_chip.codecWrite(DacVolumePage, LeftDacVolume, DacVolumeMinus22Db);
    }
    return m_chip.codecWrite(DacVolumePage, LeftDacVolume, DacVolume0Db) &&
        m_chip.codecWrite(MicRoutingPage, LeftMicPgaVolume, MicGain16Db) &&
        m_chip.codecWrite(AgcPage, LeftAgcControl, AgcOn | MicGain16Db) &&
        m_chip.codecWrite(AgcPage, LeftAgcMaxGain, MicGain16Db) &&
        m_chip.codecWrite(MicRoutingPage, LeftMicPositiveRouting, MicIn1Left20k);
}

bool Sct3258Baseband::setCodecTrims(std::uint8_t leftOut, std::uint8_t rightOut, std::uint8_t rightInput)
{
    return m_info.running && m_chip.codecWrite(TrimPage, LeftOutGain, leftOut) &&
        m_chip.codecWrite(TrimPage, RightOutGain, rightOut) &&
        m_chip.codecWrite(TrimPage, RightMicPgaVolume, rightInput);
}

bool Sct3258Baseband::codecWrite(std::uint8_t page, std::uint8_t reg, std::uint8_t value)
{
    return m_info.running && m_chip.codecWrite(page, reg, value);
}

void Sct3258Baseband::exchange(std::uint8_t type, std::span<const std::uint8_t> fields, unsigned waitMs,
    lib::FnRef<void(std::uint8_t type, std::span<const std::uint8_t> fields)> onPacket)
{
    m_chip.send(type, fields);
    drivers::baseband::Packet p;
    int left = 4;
    for (unsigned waited = 0; waited < waitMs && left > 0; ++waited) {
        if (m_chip.receive(p, 1)) {
            onPacket(p.type, {p.fields, p.length});
            --left;
        }
    }
}

} // namespace dmm::services
