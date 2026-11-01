// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Henri Lavikainen

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace dmm::services::rtty {

/// The audio tone pair: mark is the lower tone, the space 170 Hz above it.
enum class Tones : std::uint8_t
{
    High, // 2125/2295 Hz
    Low,  // 1275/1445 Hz
    Count,
};

/// The keying speed.
enum class Baud : std::uint8_t
{
    Baud45, // 45.45 Bd
    Baud50,
    Baud75,
    Count,
};

inline constexpr std::uint16_t ShiftHz = 170;

std::uint16_t markHz(Tones tones);
/// "2125/2295".
const char *tonesText(Tones tones);
float baudRate(Baud baud);
/// "45.45".
const char *baudText(Baud baud);

/// The figures row: the common US teleprinter one ($ ' " BEL ! & # ;) or
/// the international ITA2 one (' BEL + = and three unassigned).
enum class Figures : std::uint8_t
{
    UsTty,
    Ita2,
};

/// Codes with a meaning of their own in both shifts.
inline constexpr std::uint8_t Letters = 0x1F;
inline constexpr std::uint8_t FiguresShift = 0x1B;
inline constexpr std::uint8_t Space = 0x04;
inline constexpr std::uint8_t CarriageReturn = 0x08;
inline constexpr std::uint8_t LineFeed = 0x02;

/// Turns 5-bit ITA2 codes (the first bit sent in bit 0) into characters,
/// following the letters and figures shifts. Letters come out upper case;
/// CR, LF and BEL as '\r', '\n' and '\a'.
class Ita2Decoder
{
public:
    explicit Ita2Decoder(Figures figures = Figures::UsTty, bool unshiftOnSpace = true)
        : m_figuresRow(figures), m_unshiftOnSpace(unshiftOnSpace)
    {
    }

    /// Back to letters.
    void reset() { m_figures = false; }
    /// A space in figures goes back to letters (on at start), so a lost
    /// letters shift garbles at most one word.
    void setUnshiftOnSpace(bool on) { m_unshiftOnSpace = on; }
    void setFigures(Figures f) { m_figuresRow = f; }

    /// The character for @p code, or 0 for a shift, NUL or an unassigned
    /// code.
    char decode(std::uint8_t code);

    bool inFigures() const { return m_figures; }

private:
    Figures m_figuresRow;
    bool m_unshiftOnSpace;
    bool m_figures = false;
};

/// Turns text into ITA2 codes, with the shifts added where needed. Lower
/// case goes out as upper case; characters ITA2 lacks are left out.
class Ita2Encoder
{
public:
    explicit Ita2Encoder(Figures figures = Figures::UsTty)
        : m_figuresRow(figures)
    {
    }

    /// The receiver is taken to be in letters, and to go back to letters
    /// on a space in figures (unshift on space): the next figure gets its
    /// shift again.
    void reset() { m_figures = false; }

    /// The codes for @p c (a shift first if needed); none if @p c cannot
    /// be sent.
    std::size_t encode(char c, std::span<std::uint8_t, 2> out);

    /// Whether @p c can be sent.
    bool sendable(char c) const;

private:
    Figures m_figuresRow;
    bool m_figures = false;
};

/// Spots the callsign a station gives after "DE" in received text.
class CallSpotter
{
public:
    static constexpr std::size_t MaxCall = 10;

    void reset();
    void add(char c);

    /// The last callsign seen; empty until one is.
    std::string_view call() const { return {m_call.data(), m_callLength}; }

    /// Whether @p word looks like a callsign: letters, digits and '/',
    /// with at least one letter and one digit.
    static bool looksLikeCall(std::string_view word);

private:
    void endWord();

    std::array<char, MaxCall> m_word{};
    std::size_t m_wordLength = 0;
    bool m_wordTooLong = false;
    bool m_afterDe = false;
    std::array<char, MaxCall> m_call{};
    std::size_t m_callLength = 0;
};

} // namespace dmm::services::rtty
