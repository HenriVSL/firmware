// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Henri Lavikainen

#pragma once

#include "dmm/lib/format.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace dmm::services::text_mode {

/// Received text as screen lines. Characters wrap at Width, at the last
/// space where a word would be cut; CR, LF or CR LF starts a new line; the
/// oldest lines go when Lines are full. Other control characters are left
/// out.
class TextLines
{
public:
    static constexpr std::size_t Width = 31;
    static constexpr std::size_t Lines = 32;

    void clear();
    void add(char c);

    /// Lines held, the one being written included (never 0).
    std::size_t count() const { return m_older + 1; }
    /// The line @p back lines before the newest (0: the one being
    /// written); empty past the oldest.
    std::string_view line(std::size_t back) const;
    /// Counts the changes, for redrawing.
    std::uint32_t version() const { return m_version; }

private:
    void newLine();

    std::array<std::array<char, Width>, Lines> m_text{};
    std::array<std::uint8_t, Lines> m_length{};
    std::size_t m_newest = 0;
    // Whole lines kept before the newest; all zero at start, so a static
    // one needs no initial data.
    std::size_t m_older = 0;
    bool m_afterCr = false;
    std::uint32_t m_version = 0;
};

/// "144.600", or "144.6125" for a frequency between kHz steps.
lib::StaticString<12> frequencyText(std::uint32_t hz);

/// What the text mode's home screen is made from.
struct HomeInput
{
    std::uint32_t frequencyHz = 0;
    /// The callsign last given after "DE"; empty until one is.
    std::string_view call;
    /// The two newest lines of received text, the newest last.
    std::string_view older;
    std::string_view newest;
    /// The audio is being decoded now; a signal is detected, and the
    /// share of its power in the tones (0..1).
    bool decoding = false;
    bool signal = false;
    float quality = 0.0F;
    /// The tone pair and speed: "2125/2295", "45.45"; mark and space
    /// swapped.
    std::string_view tones;
    std::string_view baud;
    bool reverse = false;
    /// Characters received, and characters dropped for a framing error.
    std::uint32_t characters = 0;
    std::uint32_t errors = 0;
};

/// The home screen's rows as text.
struct HomeRows
{
    lib::StaticString<16> top;    // "RTTY 144.600"
    lib::StaticString<12> big;    // the callsign, "Receiving" or "Listening"
    lib::StaticString<32> older;  // the panel's two lines
    lib::StaticString<32> newest; //
    lib::StaticString<12> tones;  // "2125/2295", "2295/2125" reversed
    lib::StaticString<8> quality; // "72%"
    bool qualityWarn = false;     // the decoder is not running
    lib::StaticString<32> bottom; // "45.45 Bd  123 chr  4 err"
};

HomeRows homeRows(const HomeInput &in);

} // namespace dmm::services::text_mode
