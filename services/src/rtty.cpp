// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Henri Lavikainen

#include "dmm/services/rtty.h"

namespace dmm::services::rtty {

namespace {

// Indexed by code, the first bit sent in bit 0. 0 marks a shift, NUL or an
// unassigned code.
constexpr char LettersRow[32] = {
    0, 'E', '\n', 'A', ' ', 'S', 'I', 'U', '\r', 'D', 'R', 'J', 'N', 'F', 'C', 'K',
    'T', 'Z', 'L', 'W', 'H', 'Y', 'P', 'Q', 'O', 'B', 'G', 0, 'M', 'X', 'V', 0};
constexpr char UsFiguresRow[32] = {
    0, '3', '\n', '-', ' ', '\a', '8', '7', '\r', '$', '4', '\'', ',', '!', ':', '(',
    '5', '"', ')', '2', '#', '6', '0', '1', '9', '?', '&', 0, '.', '/', ';', 0};
// The international row: WRU (who are you) is left out with the
// unassigned codes.
constexpr char Ita2FiguresRow[32] = {
    0, '3', '\n', '-', ' ', '\'', '8', '7', '\r', 0, '4', '\a', ',', 0, ':', '(',
    '5', '+', ')', '2', 0, '6', '0', '1', '9', '?', 0, 0, '.', '/', '=', 0};

const char *figuresRow(Figures f)
{
    return f == Figures::Ita2 ? Ita2FiguresRow : UsFiguresRow;
}

// The code of @p c in @p row, or -1.
int find(const char *row, char c)
{
    for (int i = 0; i < 32; ++i) {
        if (row[i] == c && c != 0) {
            return i;
        }
    }
    return -1;
}

char upper(char c)
{
    return c >= 'a' && c <= 'z' ? static_cast<char>(c - 'a' + 'A') : c;
}

} // namespace

std::uint16_t markHz(Tones tones)
{
    return tones == Tones::Low ? 1275 : 2125;
}

const char *tonesText(Tones tones)
{
    return tones == Tones::Low ? "1275/1445" : "2125/2295";
}

float baudRate(Baud baud)
{
    switch (baud) {
    case Baud::Baud50:
        return 50.0F;
    case Baud::Baud75:
        return 75.0F;
    case Baud::Baud45:
    case Baud::Count:
        break;
    }
    return 45.45F;
}

const char *baudText(Baud baud)
{
    switch (baud) {
    case Baud::Baud50:
        return "50";
    case Baud::Baud75:
        return "75";
    case Baud::Baud45:
    case Baud::Count:
        break;
    }
    return "45.45";
}

char Ita2Decoder::decode(std::uint8_t code)
{
    code &= 0x1F;
    if (code == Letters) {
        m_figures = false;
        return 0;
    }
    if (code == FiguresShift) {
        m_figures = true;
        return 0;
    }
    const char c = (m_figures ? figuresRow(m_figuresRow) : LettersRow)[code];
    if (code == Space && m_unshiftOnSpace) {
        m_figures = false;
    }
    return c;
}

bool Ita2Encoder::sendable(char c) const
{
    c = upper(c);
    return find(LettersRow, c) >= 0 || find(figuresRow(m_figuresRow), c) >= 0;
}

std::size_t Ita2Encoder::encode(char c, std::span<std::uint8_t, 2> out)
{
    c = upper(c);
    const int letter = find(LettersRow, c);
    const int figure = find(figuresRow(m_figuresRow), c);
    if (letter < 0 && figure < 0) {
        return 0;
    }
    std::size_t n = 0;
    // Space, CR and LF are in both rows and keep the shift, except that a
    // space unshifts the receiver.
    if (letter >= 0 && figure >= 0) {
        out[n++] = static_cast<std::uint8_t>(letter);
        if (letter == Space) {
            m_figures = false;
        }
        return n;
    }
    if (letter >= 0) {
        if (m_figures) {
            out[n++] = Letters;
            m_figures = false;
        }
        out[n++] = static_cast<std::uint8_t>(letter);
        return n;
    }
    if (!m_figures) {
        out[n++] = FiguresShift;
        m_figures = true;
    }
    out[n++] = static_cast<std::uint8_t>(figure);
    return n;
}

std::size_t encodeTransmission(std::string_view text, std::span<std::uint8_t> out, Figures figures, Nordic nordic)
{
    Ita2Encoder encoder(figures);
    std::size_t n = 0;
    bool fits = true;
    auto put = [&](std::uint8_t code) {
        if (n < out.size()) {
            out[n++] = code;
        } else {
            fits = false;
        }
    };
    auto send = [&](char c) {
        std::uint8_t codes[2];
        const std::size_t k = encoder.encode(c, codes);
        for (std::size_t i = 0; i < k; ++i) {
            put(codes[i]);
        }
    };
    put(Letters);
    send('\r');
    send('\n');
    bool afterCr = false;
    for (std::size_t i = 0; i < text.size(); ++i) {
        const char c = text[i];
        if (c == '\n' && afterCr) {
            afterCr = false;
            continue;
        }
        afterCr = c == '\r';
        if (c == '\r' || c == '\n') {
            send('\r');
            send('\n');
            continue;
        }
        // Ä Å Ö ä å ö in UTF-8: 0xC3 then 0x84, 0x85, 0x96, 0xA4, 0xA5, 0xB6.
        if (static_cast<unsigned char>(c) == 0xC3 && i + 1 < text.size()) {
            const unsigned char next = static_cast<unsigned char>(text[i + 1]) & 0xDF;
            const char *spelt = next == 0x84 ? (nordic == Nordic::Telex ? "AE" : "A")
                : next == 0x85               ? (nordic == Nordic::Telex ? "AA" : "A")
                : next == 0x96               ? (nordic == Nordic::Telex ? "OE" : "O")
                                             : nullptr;
            if (spelt != nullptr) {
                ++i;
                for (; *spelt != '\0'; ++spelt) {
                    send(*spelt);
                }
                continue;
            }
        }
        send(c);
    }
    send('\r');
    send('\n');
    return fits ? n : 0;
}

std::uint32_t charactersMs(std::size_t characters, float baud)
{
    if (baud <= 0.0F) {
        return 0;
    }
    const float ms = static_cast<float>(characters) * 7.5F * 1000.0F / baud;
    return static_cast<std::uint32_t>(ms) + 1;
}

void CallSpotter::reset()
{
    m_wordLength = 0;
    m_wordTooLong = false;
    m_afterDe = false;
    m_callLength = 0;
}

bool CallSpotter::looksLikeCall(std::string_view word)
{
    if (word.size() < 3 || word.size() > MaxCall || word.front() == '/' || word.back() == '/') {
        return false;
    }
    bool letter = false;
    bool digit = false;
    for (const char c : word) {
        if (c >= 'A' && c <= 'Z') {
            letter = true;
        } else if (c >= '0' && c <= '9') {
            digit = true;
        } else if (c != '/') {
            return false;
        }
    }
    return letter && digit;
}

void CallSpotter::endWord()
{
    // "N0CALL." and "N0CALL," end a sentence.
    while (m_wordLength > 0 && std::string_view(".,:;?!").find(m_word[m_wordLength - 1]) != std::string_view::npos) {
        --m_wordLength;
    }
    const std::string_view word(m_word.data(), m_wordLength);
    if (m_wordTooLong) {
        m_afterDe = false;
    } else if (m_wordLength > 0) {
        if (m_afterDe && looksLikeCall(word)) {
            for (std::size_t i = 0; i < m_wordLength; ++i) {
                m_call[i] = m_word[i];
            }
            m_callLength = m_wordLength;
        }
        m_afterDe = word == "DE";
    }
    m_wordLength = 0;
    m_wordTooLong = false;
}

void CallSpotter::add(char c)
{
    c = upper(c);
    if (c == ' ' || c == '\r' || c == '\n') {
        endWord();
        return;
    }
    if (m_wordLength == MaxCall) {
        m_wordTooLong = true;
        return;
    }
    m_word[m_wordLength++] = c;
}

} // namespace dmm::services::rtty
