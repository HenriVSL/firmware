// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Henri Lavikainen

#include "gps.h"

#include "radio.h"
#include "ui.h"

#include "dmm/lib/format.h"
#include "dmm/lib/variant.h"
#include "dmm/os/os.h"
#include "dmm/services/log.h"
#include "dmm/services/nmea.h"

namespace dmm::app::gps {

namespace {

namespace log = services::log;
using services::GpsFix;
using services::NmeaParser;

board::Gps *port = nullptr;
board::GpsReceiver receiver;
os::Task<640> gpsTask;
os::Queue<Message, 4> inbox;

constexpr os::Ticks PollMs = 50;
// Silence after power-on, or between sentences, that counts as no data.
constexpr os::Ticks SilentMs = 5000;

void logFix(const GpsFix &f, std::string_view what)
{
    auto line = log::info("gps");
    line << what;
    if (f.valid) {
        line << services::latitudeText(f.latitudeE7).view() << ", " << services::longitudeText(f.longitudeE7).view() << ", "
             << f.altitudeDm / 10 << " m, ";
    } else {
        line << "no fix, ";
    }
    line << f.satellitesUsed << " of " << f.satellitesInView << " satellites";
    if (f.timeValid) {
        line << ", " << lib::zeroPadded(f.hour, 2) << ':' << lib::zeroPadded(f.minute, 2) << ':' << lib::zeroPadded(f.second, 2)
             << " UTC";
    }
}

void gpsMain(void *)
{
    NmeaParser parser;
    bool on = false;
    bool raw = false;
    bool hadFix = false;
    bool fixChangePending = false;
    bool silentReported = false;
    std::uint32_t bytes = 0;
    std::uint32_t sentences = 0;
    os::Ticks lastData = 0;
    std::uint8_t systems = services::gps_systems::Gps | services::gps_systems::Beidou;
    // The receiver is set up once it answers after power-on.
    bool configured = false;
    lib::StaticString<NmeaParser::MaxSentence> lastText;

    auto write = [&](std::span<const std::uint8_t> data) { port->write(data); };
    auto configure = [&] {
        receiver.configure(systems, write);
        configured = true;
        log::info("gps") << "receiver set up: " << services::gps_systems::name(systems);
    };

    auto tellUi = [&] { ui::post(ui::Gps{parser.fix(), on, on && !silentReported}); };

    while (!os::stopRequested()) {
        Message message;
        if (inbox.receive(message, PollMs)) {
            lib::visit(lib::Overload{
                           [&](const Power &p) {
                               if (p.on == on) {
                                   return;
                               }
                               on = p.on;
                               if (on) {
                                   radio::post(radio::GpsPower{true});
                                   port->start();
                                   parser = NmeaParser();
                                   hadFix = false;
                                   fixChangePending = false;
                                   silentReported = false;
                                   configured = false;
                                   lastText.clear();
                                   lastData = os::nowMs();
                               } else {
                                   port->stop();
                                   radio::post(radio::GpsPower{false});
                               }
                               log::info("gps") << (on ? "on" : "off");
                               tellUi();
                           },
                           [&](const Report &) {
                               log::info("gps") << (on ? "on" : "off") << ", " << bytes << " bytes, " << sentences
                                                << " sentences, " << parser.badChecksums() << " bad, " << port->lostBytes()
                                                << " lost";
                               logFix(parser.fix(), "");
                               log::info("gps") << "systems " << services::gps_systems::name(systems)
                                                << (configured ? "" : " (receiver not set up yet)");
                           },
                           [&](const Raw &r) {
                               raw = r.on;
                               log::info("gps") << "raw sentences " << (raw ? "on" : "off");
                           },
                           [&](const Systems &s) {
                               systems = s.systems;
                               if (configured) {
                                   configure();
                               } else {
                                   log::info("gps") << "systems " << services::gps_systems::name(systems);
                               }
                           },
                           [&](const ChipCommand &c) {
                               if (!on) {
                                   log::info("gps") << "the receiver is off";
                               } else if (!receiver.debugCommand(c.text.view(), write)) {
                                   log::info("gps") << "unknown command, try help";
                               }
                           },
                       },
                message);
        }
        if (!on) {
            continue;
        }
        std::uint8_t buffer[128];
        for (;;) {
            const std::size_t n = port->read(buffer);
            if (n == 0) {
                break;
            }
            bytes += static_cast<std::uint32_t>(n);
            lastData = os::nowMs();
            for (std::size_t i = 0; i < n; ++i) {
                const NmeaParser::Sentence s = parser.feed(static_cast<char>(buffer[i]));
                if (s == NmeaParser::Sentence::None) {
                    continue;
                }
                if (s != NmeaParser::Sentence::BadChecksum) {
                    ++sentences;
                }
                if (s != NmeaParser::Sentence::BadChecksum && !configured) {
                    configure();
                }
                if (raw) {
                    log::info("gps") << parser.lastSentence();
                } else if (s == NmeaParser::Sentence::Txt && parser.lastSentence() != lastText.view()) {
                    // Answers to commands, and status messages; a repeat is not logged again.
                    lastText.clear();
                    lastText << parser.lastSentence();
                    log::info("gps") << parser.lastSentence();
                }
                if (s != NmeaParser::Sentence::Rmc && s != NmeaParser::Sentence::Gga) {
                    continue;
                }
                if (silentReported) {
                    silentReported = false;
                    log::info("gps") << "receiver sending again";
                }
                // A change of fix shows in RMC; it is logged once the GGA
                // with the same epoch's altitude and satellites has come too.
                if (s == NmeaParser::Sentence::Gga && fixChangePending) {
                    fixChangePending = false;
                    logFix(parser.fix(), hadFix ? "fix: " : "fix lost: ");
                }
                if (s == NmeaParser::Sentence::Rmc && parser.fix().valid != hadFix) {
                    hadFix = parser.fix().valid;
                    fixChangePending = true;
                }
                tellUi();
            }
        }
        if (!silentReported && os::nowMs() - lastData >= SilentMs) {
            silentReported = true;
            log::warning("gps") << "no data from the receiver for " << SilentMs / 1000 << " s (" << bytes << " bytes so far)";
            tellUi();
        }
    }
}

} // namespace

void start(board::Gps &p)
{
    port = &p;
    gpsTask.start("gps", 3, gpsMain, nullptr);
}

const char *debugHelp()
{
    return receiver.debugHelp();
}

bool post(const Message &message)
{
    if (port == nullptr) {
        return false;
    }
    return inbox.send(message);
}

} // namespace dmm::app::gps
