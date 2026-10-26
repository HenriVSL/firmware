// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Henri Lavikainen

#include "dmm/services/casic_receiver.h"

#include "dmm/lib/strings.h"

#include <span>

namespace dmm::services {

namespace {

// PCAS03 output rates in its field order: GGA, GLL, GSA, GSV, RMC, VTG, ZDA,
// antenna status (TXT). 1 = every fix, 0 = never.
constexpr std::string_view OutputRates = "PCAS03,1,0,0,1,1,0,0,0";

} // namespace

lib::StaticString<100> CasicReceiver::sentence(std::string_view body)
{
    std::uint8_t sum = 0;
    for (char c : body) {
        sum = static_cast<std::uint8_t>(sum ^ static_cast<std::uint8_t>(c));
    }
    constexpr char Hex[] = "0123456789ABCDEF";
    lib::StaticString<100> s;
    s << '$' << body << '*' << Hex[sum >> 4] << Hex[sum & 15] << "\r\n";
    return s;
}

void CasicReceiver::send(std::string_view body, GpsWrite write)
{
    const auto s = sentence(body);
    const std::string_view text = s.view();
    write(std::span(reinterpret_cast<const std::uint8_t *>(text.data()), text.size()));
}

void CasicReceiver::configure(std::uint8_t systems, GpsWrite write)
{
    send(OutputRates, write);
    // PCAS04's mode uses the same bits as gps_systems.
    const std::uint8_t mode = systems == 0 || systems > gps_systems::All ? gps_systems::Gps | gps_systems::Beidou : systems;
    lib::StaticString<16> body;
    body << "PCAS04," << mode;
    send(body.view(), write);
}

bool CasicReceiver::debugCommand(std::string_view command, GpsWrite write)
{
    if (lib::startsWith(command, "send ")) {
        command.remove_prefix(5);
        if (!command.empty() && command.front() == '$') {
            command.remove_prefix(1);
        }
        if (command.empty()) {
            return false;
        }
        send(command, write);
        return true;
    }
    if (command == "query") {
        send("PCAS06,0", write);
        return true;
    }
    constexpr std::string_view Restarts[] = {"restart hot", "restart warm", "restart cold", "restart factory"};
    for (int i = 0; i < 4; ++i) {
        if (command == Restarts[i]) {
            lib::StaticString<16> body;
            body << "PCAS10," << i;
            send(body.view(), write);
            return true;
        }
    }
    return false;
}

const char *CasicReceiver::debugHelp() const
{
    return "gps send <sentence>, gps query, gps restart hot|warm|cold|factory";
}

} // namespace dmm::services
