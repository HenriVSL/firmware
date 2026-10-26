// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Henri Lavikainen

#pragma once

#include "dmm/lib/format.h"

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace dmm::services {

/// What a GPS receiver last reported. Angles and distances are scaled
/// integers, so no floating point is needed to keep or show them.
struct GpsFix
{
    /// The receiver has a position (RMC status A).
    bool valid = false;
    /// GGA fix quality: 0 none, 1 GPS, 2 differential, 6 estimated.
    std::uint8_t quality = 0;
    std::uint8_t satellitesUsed = 0;
    /// Satellites in view, summed over the satellite systems that report them.
    std::uint8_t satellitesInView = 0;
    /// Degrees times 10^7; north and east positive.
    std::int32_t latitudeE7 = 0;
    std::int32_t longitudeE7 = 0;
    /// Height above mean sea level in decimetres.
    std::int32_t altitudeDm = 0;
    /// Speed over ground in 0.1 km/h, course over ground in 0.1 degrees.
    std::uint16_t speedDeciKmh = 0;
    std::uint16_t courseDeciDeg = 0;
    /// Horizontal dilution of precision times 100; 0 = unknown.
    std::uint16_t hdopCenti = 0;
    /// UTC time and date; each valid once a sentence carried it.
    bool timeValid = false;
    std::uint8_t hour = 0;
    std::uint8_t minute = 0;
    std::uint8_t second = 0;
    bool dateValid = false;
    std::uint8_t day = 0;
    std::uint8_t month = 0;
    std::uint16_t year = 0;
};

/// "60.12345 N": degrees with five decimals (about 1 m) and the hemisphere.
lib::StaticString<16> latitudeText(std::int32_t e7);
/// "24.12345 E", likewise.
lib::StaticString<16> longitudeText(std::int32_t e7);

/// Assembles NMEA 0183 sentences from a receiver's byte stream, checks
/// their checksums and keeps the latest fix from RMC, GGA and GSV, from any
/// satellite system (talker GP, GN, GL, BD, GB or GA).
class NmeaParser
{
public:
    /// Longest sentence kept, from '$' to the checksum; longer ones are dropped.
    static constexpr std::size_t MaxSentence = 100;

    enum class Sentence : std::uint8_t
    {
        None,        // the byte did not end a sentence
        Rmc,         // position, speed, course, time and date
        Gga,         // fix quality, satellites used, altitude
        Gsv,         // satellites in view
        Txt,         // a text message from the receiver
        Other,       // a valid sentence of another type
        BadChecksum, // a sentence whose checksum did not match
    };

    /// Takes one byte; when it ends a sentence, says which kind it was.
    Sentence feed(char c);

    const GpsFix &fix() const { return m_fix; }

    /// The last complete sentence, from '$' to the checksum, valid until
    /// the next byte is fed.
    std::string_view lastSentence() const { return {m_line, m_lastLength}; }

    std::uint32_t badChecksums() const { return m_badChecksums; }

private:
    Sentence finish();
    void parseRmc(std::string_view fields);
    void parseGga(std::string_view fields);
    void parseGsv(std::string_view talker, std::string_view fields);

    GpsFix m_fix;
    char m_line[MaxSentence] = {};
    std::size_t m_length = 0;
    std::size_t m_lastLength = 0;
    bool m_inSentence = false;
    bool m_overflow = false;
    std::uint32_t m_badChecksums = 0;
    // Satellites in view per system: GPS, GLONASS, BeiDou, Galileo.
    std::uint8_t m_inView[4] = {};
};

} // namespace dmm::services
