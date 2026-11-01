// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Henri Lavikainen

#pragma once

#include <cstdint>

/// The TLV320AIC3204 audio codec's registers (page, register) and value
/// codes, from TI's datasheet (SLOS602) and application reference guide
/// (SLAA557). The codec sits behind the SCT3258 baseband, which writes
/// these registers for its host.
namespace dmm::drivers::baseband::tlv320aic3204 {

inline constexpr std::uint8_t Page0 = 0;
inline constexpr std::uint8_t Page1 = 1;

// Page 0.
inline constexpr std::uint8_t LeftDacVolume = 65;  // signed, 0.5 dB steps
inline constexpr std::uint8_t LeftAgcControl = 86; // on, target level, gain hysteresis
inline constexpr std::uint8_t LeftAgcMaxGain = 88; // 0.5 dB steps, 0..58 dB

// Page 1.
inline constexpr std::uint8_t HplGain = 16; // headphone-left driver gain, 6-bit signed dB
inline constexpr std::uint8_t LeftOutGain = 18;
inline constexpr std::uint8_t RightOutGain = 19;
inline constexpr std::uint8_t LeftMicPositive = 52; // left microphone amplifier inputs; 0 = none
inline constexpr std::uint8_t LeftMicPgaVolume = 59;
inline constexpr std::uint8_t RightMicPgaVolume = 60;

inline constexpr std::uint8_t HplMuted = 0x40;
/// IN1L on the left microphone amplifier through 20 kOhm: the amplifier's
/// gain is then 6 dB less than its volume setting says.
inline constexpr std::uint8_t MicIn1Left20k = 0x80;
inline constexpr std::uint8_t AgcOn = 0x80;
/// AGC target levels for the average of the ADC output.
inline constexpr std::uint8_t AgcTargetMinus10Db = 0x20;

/// The microphone amplifier reaches 47.5 dB, in 0.5 dB steps.
inline constexpr int MicPgaMaxDb = 47;
/// The AGC may apply up to 58 dB.
inline constexpr int AgcMaxGainMaxDb = 58;

/// The left or right microphone amplifier volume for @p db, limited to
/// 0..MicPgaMaxDb; bit 7 clear keeps the gain in use.
constexpr std::uint8_t micPgaVolume(int db)
{
    const int d = db < 0 ? 0 : db > MicPgaMaxDb ? MicPgaMaxDb
                                                : db;
    return static_cast<std::uint8_t>(2 * d);
}

/// The AGC's largest gain for @p db, limited to 0..AgcMaxGainMaxDb.
constexpr std::uint8_t agcMaxGain(int db)
{
    const int d = db < 0 ? 0 : db > AgcMaxGainMaxDb ? AgcMaxGainMaxDb
                                                    : db;
    return static_cast<std::uint8_t>(2 * d);
}

/// The headphone-left driver gain field for @p db (-6..+29).
constexpr std::uint8_t hplDb(int db)
{
    return static_cast<std::uint8_t>(db & 0x3F);
}

} // namespace dmm::drivers::baseband::tlv320aic3204
