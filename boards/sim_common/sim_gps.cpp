// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Henri Lavikainen

#include "dmm/board/sim_gps.h"

#include "dmm/lib/format.h"

#include <string_view>

namespace {

constexpr std::uint32_t SearchSeconds = 5;
// The Royal Observatory, Greenwich, as NMEA writes it.
constexpr std::string_view Latitude = "5128.67280,N";
constexpr std::string_view Longitude = "00000.08820,W";

} // namespace

void SimGps::start()
{
    m_running = true;
    m_startMs = dmm::os::nowMs();
    m_nextSecond = 0;
    m_pendingLength = m_pendingRead = 0;
}

std::size_t SimGps::read(std::span<std::uint8_t> data)
{
    if (!m_running) {
        return 0;
    }
    const auto second = static_cast<std::uint32_t>((dmm::os::nowMs() - m_startMs) / 1000);
    if (m_pendingRead == m_pendingLength && second >= m_nextSecond) {
        queueSecond(second);
        m_nextSecond = second + 1;
    }
    std::size_t n = 0;
    while (n < data.size() && m_pendingRead < m_pendingLength) {
        data[n++] = static_cast<std::uint8_t>(m_pending[m_pendingRead++]);
    }
    return n;
}

void SimGps::queueSecond(std::uint32_t second)
{
    dmm::lib::TextWriter out(m_pending);
    auto sentence = [&](const dmm::lib::TextWriter &body) {
        std::uint8_t sum = 0;
        for (char c : body.view()) {
            sum = static_cast<std::uint8_t>(sum ^ static_cast<std::uint8_t>(c));
        }
        out << '$' << body.view() << '*' << dmm::lib::hex(sum, 2) << "\r\n";
    };
    const bool fix = second >= SearchSeconds;
    // 12:00:00 UTC on the first of January, 2026, plus the seconds since start.
    const std::uint32_t t = 12 * 3600 + second;
    dmm::lib::StaticString<12> time;
    time << dmm::lib::zeroPadded(t / 3600 % 24, 2) << dmm::lib::zeroPadded(t / 60 % 60, 2) << dmm::lib::zeroPadded(t % 60, 2) << ".00";

    char buffer[100];
    dmm::lib::TextWriter rmc(buffer);
    rmc << "GNRMC," << time.view() << (fix ? ",A," : ",V,") << (fix ? Latitude : ",") << ',' << (fix ? Longitude : ",")
        << ",0.12,45.0,010126,,,A";
    sentence(rmc);
    char buffer2[100];
    dmm::lib::TextWriter gga(buffer2);
    gga << "GNGGA," << time.view() << ',' << (fix ? Latitude : ",") << ',' << (fix ? Longitude : ",") << (fix ? ",1,08,0.9,46.0,M,47.0,M,," : ",0,00,,,M,,M,,");
    sentence(gga);
    char buffer3[100];
    dmm::lib::TextWriter gsv(buffer3);
    gsv << "GPGSV,1,1,09,01,40,083,46,02,17,308,41,12,07,344,39,14,22,228,45";
    sentence(gsv);
    char buffer4[100];
    dmm::lib::TextWriter bdgsv(buffer4);
    bdgsv << "BDGSV,1,1,03,06,45,120,38,09,30,200,35,16,12,300,30";
    sentence(bdgsv);
    m_pendingLength = out.size();
    m_pendingRead = 0;
}
