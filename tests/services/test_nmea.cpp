// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Henri Lavikainen

#include "dmm/services/nmea.h"

#include <doctest/doctest.h>

#include <string>

using namespace dmm::services;
using Sentence = NmeaParser::Sentence;

namespace {

// "$" body "*" checksum.
std::string sentence(const std::string &body)
{
    unsigned sum = 0;
    for (char c : body) {
        sum ^= static_cast<unsigned char>(c);
    }
    const char *hex = "0123456789ABCDEF";
    return "$" + body + "*" + hex[sum >> 4] + hex[sum & 15];
}

Sentence feedLine(NmeaParser &p, const std::string &line)
{
    Sentence last = Sentence::None;
    for (char c : line + "\r\n") {
        const Sentence s = p.feed(c);
        if (s != Sentence::None) {
            last = s;
        }
    }
    return last;
}

} // namespace

TEST_CASE("nmea RMC gives position, speed, course, time and date")
{
    NmeaParser p;
    CHECK(feedLine(p, "$GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W*6A") == Sentence::Rmc);
    const GpsFix &f = p.fix();
    CHECK(f.valid);
    CHECK(f.latitudeE7 == 481173000);
    CHECK(f.longitudeE7 == 115166666);
    CHECK(f.speedDeciKmh == 414); // 22.4 kn
    CHECK(f.courseDeciDeg == 844);
    CHECK(f.timeValid);
    CHECK(f.hour == 12);
    CHECK(f.minute == 35);
    CHECK(f.second == 19);
    CHECK(f.dateValid);
    CHECK(f.day == 23);
    CHECK(f.month == 3);
    CHECK(f.year == 2094);
    CHECK(p.lastSentence() == "$GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W*6A");
}

TEST_CASE("nmea GGA gives quality, satellites, HDOP and altitude")
{
    NmeaParser p;
    CHECK(feedLine(p, "$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47") == Sentence::Gga);
    const GpsFix &f = p.fix();
    CHECK(f.quality == 1);
    CHECK(f.satellitesUsed == 8);
    CHECK(f.hdopCenti == 90);
    CHECK(f.altitudeDm == 5454);
    CHECK_FALSE(f.valid); // only RMC sets the position
}

TEST_CASE("nmea south and west are negative, any talker counts")
{
    NmeaParser p;
    CHECK(feedLine(p, sentence("GNRMC,010203.000,A,3351.12345,S,15112.50000,W,0.00,0.0,010126,,,A")) == Sentence::Rmc);
    CHECK(p.fix().valid);
    CHECK(p.fix().latitudeE7 == -338520575);
    CHECK(p.fix().longitudeE7 == -1512083333);
    CHECK(p.fix().year == 2026);
}

TEST_CASE("nmea angles as text")
{
    CHECK(latitudeText(481173000).view() == "48.11730 N");
    CHECK(longitudeText(-1512083333).view() == "151.20833 W");
    CHECK(latitudeText(-5000).view() == "0.00050 S");
    CHECK(longitudeText(0).view() == "0.00000 E");
}

TEST_CASE("nmea status V keeps no position")
{
    NmeaParser p;
    feedLine(p, "$GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W*6A");
    CHECK(feedLine(p, sentence("GPRMC,123520.00,V,,,,,,,230394,,,N")) == Sentence::Rmc);
    CHECK_FALSE(p.fix().valid);
    CHECK(p.fix().second == 20);
}

TEST_CASE("nmea satellites in view add up over systems")
{
    NmeaParser p;
    CHECK(feedLine(p, sentence("GPGSV,3,1,11,01,40,083,46,02,17,308,41,12,07,344,39,14,22,228,45")) == Sentence::Gsv);
    CHECK(p.fix().satellitesInView == 11);
    feedLine(p, sentence("BDGSV,1,1,04,01,40,083,46,02,17,308,41,03,07,344,39,04,22,228,45"));
    CHECK(p.fix().satellitesInView == 15);
    feedLine(p, sentence("GPGSV,1,1,09,01,40,083,46"));
    CHECK(p.fix().satellitesInView == 13);
}

TEST_CASE("nmea rejects bad checksums and noise")
{
    NmeaParser p;
    CHECK(feedLine(p, "$GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W*6B") == Sentence::BadChecksum);
    CHECK(feedLine(p, "$GPRMC,123519,A,4807.038,N") == Sentence::BadChecksum);
    CHECK(p.badChecksums() == 2);
    CHECK_FALSE(p.fix().valid);
    CHECK(feedLine(p, "noise without a dollar") == Sentence::None);
    CHECK(feedLine(p, sentence("GPTXT,01,01,02,ANTENNA OK")) == Sentence::Other);
    CHECK(feedLine(p, "$" + std::string(200, 'A')) == Sentence::None);
}

TEST_CASE("nmea sentences split across feeds and joined without line ends")
{
    NmeaParser p;
    const std::string rmc = "$GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W*6A";
    // A sentence cut short by the next '$' is dropped, the next one counts.
    for (char c : std::string("$GPGGA,1235") + rmc + "\n") {
        p.feed(c);
    }
    CHECK(p.fix().valid);
    CHECK(p.badChecksums() == 0);
}
