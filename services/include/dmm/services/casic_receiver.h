// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Henri Lavikainen

#pragma once

#include "dmm/lib/format.h"
#include "dmm/services/gps.h"

#include <cstdint>
#include <string_view>

namespace dmm::services {

/// A GPS receiver that speaks the CASIC command set ($PCAS sentences, from
/// CASIC's multimode receiver protocol specification), such as the AT6558.
/// Builds the sentences; the caller's write function sends them.
class CasicReceiver
{
public:
    /// Output: GGA, GSV and RMC once a second, nothing else. Systems:
    /// @p systems (gps_systems bits; 0 or more than All means GPS and BeiDou).
    void configure(std::uint8_t systems, GpsWrite write);

    /// "send <body>": one sentence, '$' and checksum added; "query": the
    /// firmware version (the receiver answers in a TXT sentence);
    /// "restart hot|warm|cold|factory".
    bool debugCommand(std::string_view command, GpsWrite write);
    const char *debugHelp() const;

    /// "$" @p body "*" checksum, CR LF.
    static lib::StaticString<100> sentence(std::string_view body);

private:
    static void send(std::string_view body, GpsWrite write);
};

} // namespace dmm::services
