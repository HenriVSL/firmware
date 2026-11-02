// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Henri Lavikainen

#pragma once

#include "dmm/services/codeplug.h"
#include "dmm/services/text_mode.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace dmm::app::text_mode {

/// The text mode's decoder (RTTY) and what it has received. The aprs task
/// hosts the decoder, as it holds the audio sampler: one dedicated mode is
/// on at a time, so the APRS and text decoders never run together.

/// New settings for the decoder; it takes them before its next samples.
/// Any task.
void configure(const services::TextModeSettings &settings);

/// The decoder starts on fresh audio, decodes samples in mV, or has lost
/// samples (the character in progress is dropped). The aprs task only.
void startDecoding();
void decode(std::span<const std::uint16_t> mv);
void lostSamples();
void stopDecoding();

/// What the home screen and the text view show. Any task.
struct Status
{
    bool decoding = false;
    bool signal = false;
    float quality = 0.0F;
    /// How far the decoder has moved the tones, Hz.
    float afcHz = 0.0F;
    std::uint32_t characters = 0;
    std::uint32_t errors = 0;
};
Status status();

/// Changes with every character received, for redrawing. Any task.
std::uint32_t version();
/// The lines held (at least 1), and the line @p back lines before the
/// newest, copied into @p out. Any task.
std::size_t lineCount();
std::string_view line(std::size_t back, std::span<char, services::text_mode::TextLines::Width> out);
/// The last callsign given after "DE", copied into @p out. Any task.
std::string_view call(std::span<char, services::rtty::CallSpotter::MaxCall> out);

/// The console's text commands, for its help.
const char *consoleHelp();

/// Runs a "text ..." console command; false if @p command is not one.
bool runCommand(std::string_view command);

} // namespace dmm::app::text_mode
