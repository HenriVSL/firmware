// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Henri Lavikainen

#include "dmm/services/nmea.h"

namespace dmm::services {

namespace {

// The part of @p s from @p pos, at most @p n characters; empty past the
// end. Not substr(): its range check would pull in exception code.
std::string_view slice(std::string_view s, std::size_t pos, std::size_t n = std::string_view::npos)
{
    if (pos >= s.size()) {
        return {};
    }
    const std::size_t rest = s.size() - pos;
    return {s.data() + pos, n < rest ? n : rest};
}

// Splits comma-separated fields one at a time.
class Fields
{
public:
    explicit Fields(std::string_view text)
        : m_rest(text)
    {
    }

    std::string_view next()
    {
        const std::size_t comma = m_rest.find(',');
        const std::string_view field = slice(m_rest, 0, comma);
        m_rest = comma == std::string_view::npos ? std::string_view() : slice(m_rest, comma + 1);
        return field;
    }

private:
    std::string_view m_rest;
};

int hexDigit(char c)
{
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    return -1;
}

// A decimal number with up to @p decimals fraction digits, scaled by
// 10^decimals; extra fraction digits are dropped. False when empty or not
// a number.
bool decimal(std::string_view s, int decimals, std::int64_t &out)
{
    bool negative = false;
    if (!s.empty() && s.front() == '-') {
        negative = true;
        s.remove_prefix(1);
    }
    if (s.empty()) {
        return false;
    }
    std::int64_t value = 0;
    int fraction = -1;
    for (char c : s) {
        if (c == '.') {
            if (fraction >= 0) {
                return false;
            }
            fraction = 0;
            continue;
        }
        if (c < '0' || c > '9') {
            return false;
        }
        if (fraction >= decimals) {
            continue;
        }
        value = value * 10 + (c - '0');
        if (fraction >= 0) {
            ++fraction;
        }
    }
    for (int i = fraction < 0 ? 0 : fraction; i < decimals; ++i) {
        value *= 10;
    }
    out = negative ? -value : value;
    return true;
}

// "ddmm.mmmmm" or "dddmm.mmmmm" with its hemisphere letter, as degrees
// times 10^7.
bool angle(std::string_view value, std::string_view hemisphere, char negativeSide, std::int32_t &out)
{
    std::int64_t minutesE5 = 0;
    if (!decimal(value, 5, minutesE5) || hemisphere.size() != 1) {
        return false;
    }
    constexpr std::int64_t E5 = 100'000;
    const std::int64_t degrees = minutesE5 / (100 * E5);
    const std::int64_t minutes = minutesE5 % (100 * E5);
    const std::int64_t e7 = degrees * 10'000'000 + minutes * 100 / 60;
    out = static_cast<std::int32_t>(hemisphere.front() == negativeSide ? -e7 : e7);
    return true;
}

// "hhmmss" or "hhmmss.sss".
bool parseTime(std::string_view s, GpsFix &fix)
{
    std::int64_t v = 0;
    if (s.size() < 6 || !decimal(slice(s, 0, 6), 0, v)) {
        return false;
    }
    const auto hour = static_cast<std::uint8_t>(v / 10000);
    const auto minute = static_cast<std::uint8_t>(v / 100 % 100);
    const auto second = static_cast<std::uint8_t>(v % 100);
    if (hour > 23 || minute > 59 || second > 60) {
        return false;
    }
    fix.hour = hour;
    fix.minute = minute;
    fix.second = second;
    fix.timeValid = true;
    return true;
}

// "ddmmyy".
bool parseDate(std::string_view s, GpsFix &fix)
{
    std::int64_t v = 0;
    if (s.size() != 6 || !decimal(s, 0, v)) {
        return false;
    }
    const auto day = static_cast<std::uint8_t>(v / 10000);
    const auto month = static_cast<std::uint8_t>(v / 100 % 100);
    if (day < 1 || day > 31 || month < 1 || month > 12) {
        return false;
    }
    fix.day = day;
    fix.month = month;
    fix.year = static_cast<std::uint16_t>(2000 + v % 100);
    fix.dateValid = true;
    return true;
}

int systemIndex(std::string_view talker)
{
    if (talker == "GP") {
        return 0;
    }
    if (talker == "GL") {
        return 1;
    }
    if (talker == "BD" || talker == "GB") {
        return 2;
    }
    if (talker == "GA") {
        return 3;
    }
    return -1;
}

lib::StaticString<16> angleText(std::int32_t e7, char positive, char negative)
{
    const auto a = static_cast<std::uint32_t>(e7 < 0 ? -static_cast<std::int64_t>(e7) : e7);
    lib::StaticString<16> s;
    s << a / 10'000'000 << '.' << lib::zeroPadded(a / 100 % 100'000, 5) << ' ' << (e7 < 0 ? negative : positive);
    return s;
}

} // namespace

lib::StaticString<16> latitudeText(std::int32_t e7)
{
    return angleText(e7, 'N', 'S');
}

lib::StaticString<16> longitudeText(std::int32_t e7)
{
    return angleText(e7, 'E', 'W');
}

NmeaParser::Sentence NmeaParser::feed(char c)
{
    if (c == '$') {
        m_inSentence = true;
        m_overflow = false;
        m_length = 0;
    }
    if (!m_inSentence) {
        return Sentence::None;
    }
    if (c == '\r' || c == '\n') {
        m_inSentence = false;
        return m_overflow ? Sentence::None : finish();
    }
    if (m_length < MaxSentence) {
        m_line[m_length++] = c;
    } else {
        m_overflow = true;
    }
    return Sentence::None;
}

NmeaParser::Sentence NmeaParser::finish()
{
    m_lastLength = m_length;
    const std::string_view line(m_line, m_length);
    // "$" body "*" two hex digits.
    if (line.size() < 4 || line[line.size() - 3] != '*') {
        ++m_badChecksums;
        return Sentence::BadChecksum;
    }
    const std::string_view body = slice(line, 1, line.size() - 4);
    std::uint8_t sum = 0;
    for (char ch : body) {
        sum = static_cast<std::uint8_t>(sum ^ static_cast<std::uint8_t>(ch));
    }
    const int high = hexDigit(line[line.size() - 2]);
    const int low = hexDigit(line[line.size() - 1]);
    if (high < 0 || low < 0 || sum != high * 16 + low) {
        ++m_badChecksums;
        return Sentence::BadChecksum;
    }
    // Talker (2), type (3), then the fields.
    if (body.size() < 6 || body[5] != ',') {
        return Sentence::Other;
    }
    const std::string_view talker = slice(body, 0, 2);
    const std::string_view type = slice(body, 2, 3);
    const std::string_view fields = slice(body, 6);
    if (type == "RMC") {
        parseRmc(fields);
        return Sentence::Rmc;
    }
    if (type == "GGA") {
        parseGga(fields);
        return Sentence::Gga;
    }
    if (type == "GSV") {
        parseGsv(talker, fields);
        return Sentence::Gsv;
    }
    if (type == "TXT") {
        return Sentence::Txt;
    }
    return Sentence::Other;
}

void NmeaParser::parseRmc(std::string_view text)
{
    Fields f(text);
    parseTime(f.next(), m_fix);
    const std::string_view status = f.next();
    const std::string_view lat = f.next();
    const std::string_view ns = f.next();
    const std::string_view lon = f.next();
    const std::string_view ew = f.next();
    const std::string_view knots = f.next();
    const std::string_view course = f.next();
    parseDate(f.next(), m_fix);

    std::int32_t latitude = 0;
    std::int32_t longitude = 0;
    m_fix.valid = status == "A" && angle(lat, ns, 'S', latitude) && angle(lon, ew, 'W', longitude);
    if (!m_fix.valid) {
        return;
    }
    m_fix.latitudeE7 = latitude;
    m_fix.longitudeE7 = longitude;
    std::int64_t v = 0;
    // 1 knot = 1.852 km/h.
    m_fix.speedDeciKmh = decimal(knots, 3, v) && v >= 0 ? static_cast<std::uint16_t>(v * 1852 / 100'000) : 0;
    m_fix.courseDeciDeg = decimal(course, 1, v) && v >= 0 && v < 3600 ? static_cast<std::uint16_t>(v) : 0;
}

void NmeaParser::parseGga(std::string_view text)
{
    Fields f(text);
    parseTime(f.next(), m_fix);
    for (int i = 0; i < 4; ++i) {
        f.next(); // position: RMC carries it with its status
    }
    std::int64_t v = 0;
    m_fix.quality = decimal(f.next(), 0, v) && v >= 0 && v <= 9 ? static_cast<std::uint8_t>(v) : 0;
    m_fix.satellitesUsed = decimal(f.next(), 0, v) && v >= 0 && v <= 99 ? static_cast<std::uint8_t>(v) : 0;
    m_fix.hdopCenti = decimal(f.next(), 2, v) && v >= 0 && v <= 0xFFFF ? static_cast<std::uint16_t>(v) : 0;
    if (decimal(f.next(), 1, v)) {
        m_fix.altitudeDm = static_cast<std::int32_t>(v);
    }
}

void NmeaParser::parseGsv(std::string_view talker, std::string_view text)
{
    const int system = systemIndex(talker);
    if (system < 0) {
        return;
    }
    Fields f(text);
    f.next(); // number of messages
    f.next(); // this message's number
    std::int64_t v = 0;
    if (!decimal(f.next(), 0, v) || v < 0 || v > 99) {
        return;
    }
    m_inView[system] = static_cast<std::uint8_t>(v);
    unsigned total = 0;
    for (std::uint8_t n : m_inView) {
        total += n;
    }
    m_fix.satellitesInView = static_cast<std::uint8_t>(total > 255 ? 255 : total);
}

} // namespace dmm::services
