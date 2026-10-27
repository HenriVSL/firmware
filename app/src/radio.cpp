// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tommi Tauriainen

#include "radio.h"

#include "audio.h"
#include "system.h"
#include "ui.h"

#include "dmm/lib/format.h"
#include "dmm/lib/variant.h"
#include "dmm/os/os.h"
#include "dmm/services/ax25.h"
#include "dmm/services/battery_saver.h"
#include "dmm/services/log.h"
#include "dmm/services/power.h"
#include "dmm/services/squelch_gate.h"

#include <array>
#include <atomic>
#include <span>
#include <string_view>

namespace dmm::app::radio {

namespace {

namespace log = services::log;
namespace power = services::power;

board::Fm *fm = nullptr;
board::Baseband *baseband = nullptr;
// Room for the deepest path, a channel change down to a baseband packet,
// with an interrupt on top and a margin.
os::Task<768> radioTask;
os::Queue<Message, 8> inbox;
// The last speaker volume asked for, applied again after every start.
int speakerVolume = -1;

// Half of full scale: loud enough to hear, short of clipping.
constexpr int ChipTonePercent = 50;

// FM receive: the channel, the squelch level and the speaker gate.
constexpr os::Ticks FmPollMs = 10;
constexpr os::Ticks IdleWaitMs = 1000;
constexpr int DefaultSquelchLevel = 3;
services::FmChannel fmChannel;
int squelchLevel = DefaultSquelchLevel;
services::SquelchGate gate;
bool speakerOpen = false;
services::BatterySaver saver;

// FM transmit settings and state.
constexpr std::uint16_t DefaultTimeoutS = 30;
std::uint32_t transmitHz = 0; // 0 = the receive frequency
std::uint16_t ctcssDeciHz = 0;
services::PowerLevel powerLevel = services::PowerLevel::Low;
std::uint16_t timeoutS = DefaultTimeoutS;
bool receiveOnly = false;
bool transmitting = false;
// FM transmit audio takes the transceiver's own microphone input; adding
// the route through the baseband gives a delayed second copy.
FmTransmitAudio::Route transmitAudio = FmTransmitAudio::Route::Direct;
os::Ticks transmitStarted = 0;

// Packet radio: one frame waits at a time. sendPacket() fills the buffer
// and the radio task owns it until the frame is sent or given up.
board::Modem *modem = nullptr;
std::array<std::uint8_t, services::ax25::MaxFrameBytes> packetBytes{};
std::size_t packetLength = 0;
std::atomic<bool> packetTaken{false};
enum class PacketState : std::uint8_t
{
    None,
    Waiting,
    Sending,
};
PacketState packetState = PacketState::None;
std::uint16_t packetLeadMs = 300;
std::uint8_t packetLevelPercent = 5;
PacketChannel packetChannel;
// A packet on another frequency: the working channel to go back to.
bool hopped = false;
services::FmChannel workingChannel;
bool workingReceiving = false;
// Listening on the packet frequency before the first chance to send.
constexpr os::Ticks HopListenMs = 500;
// Channel access as AX.25 does it: once the channel is free, send in a
// slot with probability Persistence / 256, otherwise look again a slot
// later; stations that waited for the same one then rarely start together.
constexpr os::Ticks SlotMs = 100;
constexpr std::uint32_t Persistence = 63;
constexpr os::Ticks GiveUpMs = 30'000;
os::Ticks packetQueuedAt = 0;
os::Ticks nextSlot = 0;
// No sending before this: listening first after queueing or retuning.
os::Ticks listenUntil = 0;
os::Ticks packetAirMs = 0;
// The receiver heard a carrier at the last poll.
bool carrier = false;
std::uint32_t randomState = 0x2545F491;

// Transmit only inside the 2 m and 70 cm amateur bands.
struct Band
{
    std::uint32_t lowHz;
    std::uint32_t highHz;
};
constexpr Band TransmitBands[] = {{144'000'000, 146'000'000}, {430'000'000, 440'000'000}};

bool transmitAllowed(std::uint32_t hz)
{
    for (const Band &b : TransmitBands) {
        if (hz >= b.lowHz && hz <= b.highHz) {
            return true;
        }
    }
    return false;
}

const char *routeName(FmTransmitAudio::Route route)
{
    constexpr const char *Names[] = {"both routes", "through the baseband only", "straight to the transceiver only"};
    return Names[static_cast<int>(route) % 3];
}

const char *powerName(services::PowerLevel level)
{
    constexpr const char *Names[] = {"low", "mid", "high", "turbo"};
    return Names[static_cast<int>(level) & 3];
}

// The board's baseband, if it has one.
template <class F>
void withBaseband(F &&f)
{
    app::withBaseband(baseband, f);
}

// The squelch gate's result: the baseband passes receive audio only while
// told a carrier is present, and the speaker opens for it.
void setSpeaker(bool open)
{
    if (open == speakerOpen) {
        return;
    }
    speakerOpen = open;
    if (open) {
        power::hold(power::AwakeReason::Carrier);
    } else {
        power::release(power::AwakeReason::Carrier);
    }
    ui::post(ui::Receiving{open});
    withBaseband([open](auto &b) { b.setCarrier(open); });
    audio::post(audio::ReceiveAudio{open});
}

// Log lines of the receive set-up are built in their own frames, kept out
// of the deep path down to the chips.
[[gnu::noinline, gnu::cold]] void logAnalogRefused()
{
    log::warning("baseband") << "analog receive set-up not accepted";
}

// The baseband's analog FM mode for the current channel, receiving.
void basebandAnalogReceive()
{
    withBaseband([](auto &b) {
        const bool ok = b.setAnalog(fmChannel.wide) && b.setWorkMode(services::BasebandWorkMode::Receive) &&
            b.setCarrier(speakerOpen);
        if (!ok) {
            logAnalogRefused();
        }
    });
}

// MHz with three decimals, for the log.
lib::StaticString<12> mhz(std::uint32_t hz)
{
    const std::uint32_t khz = hz / 1000;
    lib::StaticString<12> text;
    text << khz / 1000 << '.' << static_cast<char>('0' + khz / 100 % 10) << static_cast<char>('0' + khz / 10 % 10)
         << static_cast<char>('0' + khz % 10);
    return text;
}

void report()
{
    if constexpr (!HasBaseband) {
        log::info("baseband") << "none on this radio";
    }
    withBaseband([](auto &b) {
        const services::BasebandInfo i = b.info();
        if (!i.running) {
            log::info("baseband") << "not running: " << (i.problem != nullptr ? i.problem : "not started")
                                  << "; data line " << (i.dataWaiting ? "waiting" : "idle");
            return;
        }
        log::info("baseband") << "running: product " << i.product << ", hardware " << i.hardware << ", software "
                              << i.software << "; data line " << (i.dataWaiting ? "waiting" : "idle");
    });
}

// Resets the FM transceiver: idle, microphone muted (it reaches the
// speaker otherwise), every RF switch off.
void startFm()
{
    if (!fm->start()) {
        log::error("fm") << "no answer from the FM transceiver";
        return;
    }
    log::info("fm") << "transceiver found; microphone muted, idle";
}

[[gnu::noinline, gnu::cold]] void logReceiveRefused(const char *problem)
{
    log::error("fm") << "receive refused at " << mhz(fmChannel.receiveHz).view() << " MHz: " << problem;
}

[[gnu::noinline]] void logReceiving()
{
    log::info("fm") << "receiving " << mhz(fmChannel.receiveHz).view() << " MHz, " << (fmChannel.wide ? "25" : "12.5")
                    << " kHz, squelch " << squelchLevel;
}

void fmReceive()
{
    if (system::calibration() == nullptr) {
        log::error("fm") << "receive refused: no calibration loaded";
        return;
    }
    const int level = squelchLevel < 1 ? 1 : squelchLevel;
    const char *problem = fm->receive(fmChannel, level);
    saver.reset();
    gate.reset();
    gate.setForcedOpen(squelchLevel == 0);
    if (problem != nullptr) {
        setSpeaker(false);
        logReceiveRefused(problem);
        return;
    }
    basebandAnalogReceive();
    logReceiving();
    setSpeaker(gate.open());
}

void fmStatus()
{
    if (!fm->receiving()) {
        log::info("fm") << "idle";
        return;
    }
    const services::FmReceiveStatus s = fm->status();
    lib::StaticString<64> what;
    what << mhz(fmChannel.receiveHz).view() << " MHz, carrier " << (s.carrier ? "yes" : "no") << ", speaker "
         << (speakerOpen ? "open" : "closed");
    fm->logStatus(what.view());
}

// One squelch poll while receiving.
void pollFm()
{
    const services::FmReceiveStatus s = fm->status();
    carrier = s.carrier;
    const bool open = gate.update(s.carrier);
    if (open != speakerOpen) {
        fm->logStatus(open ? "squelch open" : "squelch closed");
        setSpeaker(open);
    }
}

lib::StaticString<16> ctcssText()
{
    lib::StaticString<16> text;
    if (ctcssDeciHz == 0) {
        text << "no tone";
    } else {
        text << "tone " << ctcssDeciHz / 10 << '.' << static_cast<char>('0' + ctcssDeciHz % 10) << " Hz";
    }
    return text;
}

// A packet goes out simplex on the receive frequency, without a tone, at
// its own power, whatever the channel's transmit settings.
void startTransmit(bool packet = false)
{
    if (transmitting) {
        return;
    }
    if (!fm->receiving() || system::calibration() == nullptr) {
        log::info("fm") << "PTT: no FM channel set (fm rx first)";
        return;
    }
    services::FmChannel channel = fmChannel;
    channel.transmitHz = transmitHz != 0 && !packet ? transmitHz : fmChannel.receiveHz;
    const services::PowerLevel level = packet ? packetChannel.power : powerLevel;
    const std::uint16_t ctcss = packet ? 0 : static_cast<std::uint16_t>(ctcssDeciHz * 10);
    if (receiveOnly) {
        log::info("fm") << "PTT: this channel is receive only";
        return;
    }
    if (!transmitAllowed(channel.transmitHz)) {
        log::warning("fm") << "PTT: " << mhz(channel.transmitHz).view() << " MHz is outside the 2 m and 70 cm bands";
        return;
    }
    setSpeaker(false);
    withBaseband([packet](auto &b) {
        b.setMicrophone(!packet && transmitAudio != FmTransmitAudio::Route::Direct);
        b.setWorkMode(services::BasebandWorkMode::Transmit);
    });
    const char *problem = fm->transmit(channel, level, ctcss);
    if (problem != nullptr) {
        fm->endTransmit();
        withBaseband([](auto &b) { b.setMicrophone(false); });
        log::error("fm") << "transmit refused at " << mhz(channel.transmitHz).view() << " MHz: " << problem;
        fmReceive();
        return;
    }
    if (packet || transmitAudio == FmTransmitAudio::Route::Baseband) {
        fm->setMicrophoneMuted(true);
    }
    transmitting = true;
    power::hold(power::AwakeReason::Transmit);
    ui::post(ui::Transmitting{true});
    transmitStarted = os::nowMs();
    log::info("fm") << "transmitting " << mhz(channel.transmitHz).view() << " MHz, power " << powerName(level) << ", "
                    << (packet ? std::string_view("packet") : ctcssText().view()) << ", timeout " << timeoutS << " s, audio "
                    << (packet ? "from the modem" : routeName(transmitAudio));
}

void stopTransmit(const char *why)
{
    if (!transmitting) {
        return;
    }
    fm->endTransmit();
    withBaseband([](auto &b) { b.setMicrophone(false); });
    transmitting = false;
    power::release(power::AwakeReason::Transmit);
    ui::post(ui::Transmitting{false});
    log::info("fm") << "transmit off (" << why << ") after " << (os::nowMs() - transmitStarted) / 100 / 10 << "."
                    << (os::nowMs() - transmitStarted) / 100 % 10 << " s";
    fmReceive();
}

std::uint32_t random32()
{
    randomState ^= randomState << 13;
    randomState ^= randomState >> 17;
    randomState ^= randomState << 5;
    return randomState;
}

// Back to the working channel after a packet on another frequency.
void returnFromHop()
{
    if (!hopped) {
        return;
    }
    hopped = false;
    fmChannel = workingChannel;
    if (workingReceiving) {
        fmReceive();
    } else {
        fm->idle();
        setSpeaker(false);
    }
}

void endPacket(bool sent)
{
    returnFromHop();
    packetState = PacketState::None;
    packetTaken = false;
    ui::post(ui::Packet{sent});
}

// Goes to the packet's frequency, remembering the working channel.
void hop()
{
    workingChannel = fmChannel;
    workingReceiving = fm->receiving();
    hopped = true;
    fmChannel = services::FmChannel{packetChannel.hz, packetChannel.hz, packetChannel.wide};
    fmReceive();
    listenUntil = os::nowMs() + HopListenMs;
    log::info("packet") << "listening on " << mhz(packetChannel.hz).view() << " MHz";
}

void startPacket()
{
    startTransmit(true);
    if (!transmitting) {
        log::warning("packet") << "not sent: the transmitter did not start";
        endPacket(false);
        return;
    }
    if (!modem->send({packetBytes.data(), packetLength}, packetLeadMs, packetLevelPercent)) {
        stopTransmit("modem busy");
        endPacket(false);
        return;
    }
    // The longest the frame can take: every sixth bit stuffed, plus the
    // closing flags and a margin.
    packetAirMs = packetLeadMs + static_cast<os::Ticks>((packetLength + 4) * 8 * 6 / 5 * 1000 / 1200) + 200;
    packetState = PacketState::Sending;
    log::info("packet") << "sending " << packetLength << " bytes after " << (os::nowMs() - packetQueuedAt) << " ms";
}

// Moves a queued frame on: waits for a free slot, then sends it.
void packetStep()
{
    const os::Ticks now = os::nowMs();
    if (packetState == PacketState::Sending) {
        if (!modem->sending()) {
            stopTransmit("packet sent");
            endPacket(true);
        } else if (now - transmitStarted > packetAirMs) {
            stopTransmit("packet took too long");
            endPacket(false);
        }
        return;
    }
    if (packetState != PacketState::Waiting) {
        return;
    }
    if (!hopped && packetChannel.hz != 0 && (!fm->receiving() || fmChannel.receiveHz != packetChannel.hz)) {
        hop();
        return;
    }
    if (!fm->receiving()) {
        log::warning("packet") << "dropped: no FM channel";
        endPacket(false);
        return;
    }
    if (now - packetQueuedAt >= GiveUpMs) {
        log::warning("packet") << "dropped: the channel stayed busy for " << GiveUpMs / 1000 << " s";
        endPacket(false);
        return;
    }
    if (transmitting || carrier) {
        nextSlot = now + SlotMs;
        return;
    }
    if (static_cast<std::int32_t>(now - nextSlot) < 0 || static_cast<std::int32_t>(now - listenUntil) < 0) {
        return;
    }
    if ((random32() & 0xFF) >= Persistence) {
        nextSlot = now + SlotMs;
        return;
    }
    startPacket();
}

void bringUp()
{
    withBaseband([](auto &b) {
        log::info("baseband") << "starting";
        const os::Ticks started = os::nowMs();
        b.start();
        log::info("baseband") << "start took " << (os::nowMs() - started) << " ms";
        report();
        if (speakerVolume >= 0) {
            b.setSpeakerVolume(speakerVolume);
        }
        // A restart forgets the analog set-up.
        if (fm->receiving() && system::calibration() != nullptr) {
            basebandAnalogReceive();
        }
    });
}

const char *saveModeText(std::uint8_t mode)
{
    constexpr const char *Texts[] = {"off", "on, 150 ms sleep", "on, 70 ms sleep"};
    return Texts[mode <= 2 ? mode : 1];
}

void saveStatus()
{
    lib::StaticString<64> reasons;
    for (unsigned i = 0; i < static_cast<unsigned>(power::AwakeReason::Count); ++i) {
        const auto reason = static_cast<power::AwakeReason>(i);
        if (power::isHeld(reason)) {
            reasons << (reasons.view().empty() ? "" : ", ") << power::name(reason);
        }
    }
    log::info("power") << "battery save " << saveModeText(saver.mode()) << "; "
                       << (saver.active() ? (saver.asleep() ? "receiver asleep" : "listening") : "awake") << ", "
                       << saver.cycles() << " sleeps so far; awake for: "
                       << (reasons.view().empty() ? std::string_view("nothing") : reasons.view()) << ", idle "
                       << power::idleForMs() / 1000 << " s";
}

// Sleeps or wakes the receiver as battery save asks.
void saveStep()
{
    // A waiting packet needs the receiver listening for the channel.
    const bool receiverOn = !transmitting && packetState == PacketState::None && (fm->receiving() || fm->receiverAsleep());
    const bool wasActive = saver.active();
    switch (saver.update(os::nowMs(), power::idleForMs(), receiverOn)) {
    case services::BatterySaver::Action::Sleep:
        fm->sleepReceiver();
        break;
    case services::BatterySaver::Action::Wake:
        fm->wakeReceiver();
        break;
    case services::BatterySaver::Action::None:
        break;
    }
    if (saver.active() != wasActive) {
        if (saver.active()) {
            log::info("power") << "battery save: receiver sleeps " << services::BatterySaver::offMs(saver.mode())
                               << " ms, listens " << services::BatterySaver::ListenMs << " ms";
        } else {
            log::info("power") << "battery save ended after " << saver.cycles() << " sleeps so far";
        }
    }
}

void handle(Message &message)
{
    // Every command finds the receiver awake; battery save's own messages
    // leave it as it is.
    const bool saveMessage =
        std::get_if<BatterySave>(&message) != nullptr || std::get_if<BatterySaveStatus>(&message) != nullptr;
    if (saver.active() && !saveMessage) {
        if (fm->receiverAsleep()) {
            fm->wakeReceiver();
        }
        saver.reset();
        log::info("power") << "battery save ended after " << saver.cycles() << " sleeps so far";
    }
    // The user's own use of the radio comes first: a packet waiting on
    // another frequency is given up, back on the working channel.
    if (hopped && packetState == PacketState::Waiting) {
        const Ptt *ptt = std::get_if<Ptt>(&message);
        if ((ptt != nullptr && ptt->down) || std::get_if<UseChannel>(&message) != nullptr ||
            std::get_if<FmReceive>(&message) != nullptr || std::get_if<FmIdle>(&message) != nullptr) {
            log::info("packet") << "dropped: the radio is in use";
            endPacket(false);
        }
    }
    // Anything but PTT itself ends a transmission first; a queued packet
    // waits for it instead.
    if (transmitting && std::get_if<Ptt>(&message) == nullptr && std::get_if<SendPacket>(&message) == nullptr) {
        stopTransmit("another command");
    }
    lib::visit(lib::Overload{
                   [](const Info &) { report(); },
                   [](const Restart &) { bringUp(); },
                   [](const ChipTone &t) {
                       withBaseband([&t](auto &b) {
                           const bool ok = b.playTone(t.hz, t.ms, ChipTonePercent);
                           log::info("baseband") << "tone " << t.hz << " Hz, " << t.ms << " ms: " << (ok ? "accepted" : "refused");
                       });
                   },
                   [](const SpeakerVolume &v) {
                       speakerVolume = v.step;
                       withBaseband([&v](auto &b) {
                           const bool ok = b.setSpeakerVolume(v.step);
                           log::info("baseband") << "speaker volume " << v.step << ": " << (ok ? "set" : "refused");
                       });
                   },
                   [](const FmReceive &r) {
                       fmChannel = services::FmChannel{r.receiveHz, r.receiveHz, r.wide};
                       fmReceive();
                   },
                   [](const FmIdle &) {
                       fm->idle();
                       setSpeaker(false);
                       withBaseband([](auto &b) { b.setWorkMode(services::BasebandWorkMode::Idle); });
                       log::info("fm") << "idle";
                   },
                   [](const FmSquelch &s) {
                       squelchLevel = s.level;
                       if (fm->receiving()) {
                           fmReceive();
                       } else {
                           log::info("fm") << "squelch " << squelchLevel;
                       }
                   },
                   [](const FmStatus &) { fmStatus(); },
                   [](const Ptt &p) {
                       if (p.down) {
                           startTransmit();
                       } else {
                           stopTransmit("PTT released");
                       }
                   },
                   [](const FmTransmitFrequency &f) {
                       transmitHz = f.hz;
                       log::info("fm") << "transmit on "
                                       << (f.hz != 0 ? mhz(f.hz).view() : std::string_view("the receive frequency"))
                                       << (f.hz != 0 ? " MHz" : "");
                   },
                   [](const FmCtcss &c) {
                       ctcssDeciHz = c.deciHz;
                       log::info("fm") << "transmit " << ctcssText().view();
                   },
                   [](const FmPower &p) {
                       powerLevel = p.level;
                       log::info("fm") << "transmit power " << powerName(powerLevel);
                   },
                   [](const FmTransmitAudio &a) {
                       transmitAudio = a.route;
                       log::info("fm") << "transmit audio: " << routeName(a.route);
                   },
                   [](const FmTimeout &t) {
                       timeoutS = t.seconds;
                       log::info("fm") << "transmit timeout " << timeoutS << " s";
                   },
                   [](const UseChannel &u) {
                       const services::Channel &c = u.channel;
                       if (!c.analog()) {
                           fm->idle();
                           setSpeaker(false);
                           log::info("fm") << "digital channel " << c.name << ": not supported yet, receiver off";
                           return;
                       }
                       fmChannel = services::FmChannel{c.receiveHz, c.transmitHz, c.wide};
                       transmitHz = c.transmitHz;
                       ctcssDeciHz = c.transmitTone.kind == services::Tone::Kind::Ctcss ? c.transmitTone.value : 0;
                       if (c.transmitTone.kind == services::Tone::Kind::Dcs) {
                           log::warning("fm") << "DCS transmit not supported yet: sending no tone";
                       }
                       powerLevel = c.power;
                       receiveOnly = c.receiveOnly;
                       squelchLevel = u.squelch;
                       fmReceive();
                   },
                   [](const BatterySave &s) {
                       saver.setMode(s.mode);
                       log::info("power") << "battery save " << saveModeText(saver.mode());
                   },
                   [](const BatterySaveStatus &) { saveStatus(); },
                   [](const SendPacket &) {
                       packetState = PacketState::Waiting;
                       packetQueuedAt = os::nowMs();
                       // Listen for a slot before the first chance to send.
                       nextSlot = packetQueuedAt + SlotMs;
                       listenUntil = nextSlot;
                       randomState ^= packetQueuedAt;
                   },
                   [](const PacketSettings &s) {
                       packetLeadMs = s.leadMs;
                       packetLevelPercent = s.levelPercent;
                       log::info("packet") << "lead " << packetLeadMs << " ms, level " << packetLevelPercent << " %";
                   },
                   [](const Microphone &m) {
                       fm->setMicrophoneMuted(!m.on);
                       log::info("fm") << "microphone " << (m.on ? "on" : "muted");
                   },
                   [](const ChipCommand &c) {
                       bool known = false;
                       if (c.chip == ChipCommand::Chip::Fm) {
                           known = fm->debugCommand(c.text.view());
                       } else {
                           withBaseband([&](auto &b) { known = b.debugCommand(c.text.view()); });
                       }
                       if (!known) {
                           log::info("console") << "unknown command";
                       }
                   },
                   [](const GpsPower &g) {
                       fm->setGpsPower(g.on);
                       log::info("gps") << "receiver power " << (g.on ? "on" : "off");
                   },
               },
        message);
}

void radioMain(void *)
{
    startFm();
    bringUp();
    os::Ticks lastPoll = os::nowMs();
    while (!os::stopRequested()) {
        const bool busy = fm->receiving() || transmitting || packetState != PacketState::None;
        os::Ticks wait = busy ? FmPollMs : IdleWaitMs;
        if (saver.asleep()) {
            const auto left = static_cast<std::int32_t>(saver.nextChangeMs() - os::nowMs());
            wait = left > 0 ? static_cast<os::Ticks>(left) : 0;
        }
        // Commands wait while a frame goes out; any would end it.
        Message m;
        if (packetState == PacketState::Sending) {
            os::sleepMs(FmPollMs);
        } else if (inbox.receive(m, wait)) {
            handle(m);
        }
        packetStep();
        saveStep();
        if (transmitting && timeoutS != 0 && os::nowMs() - transmitStarted >= static_cast<os::Ticks>(timeoutS) * 1000) {
            stopTransmit("timeout");
        }
        if (fm->receiving() && os::nowMs() - lastPoll >= FmPollMs) {
            lastPoll = os::nowMs();
            pollFm();
        }
    }
}

} // namespace

void start(board::Fm &f, board::Baseband *b, board::Modem *m)
{
    fm = &f;
    baseband = b;
    modem = m;
    radioTask.start("radio", 7, radioMain, nullptr);
}

bool post(const Message &message)
{
    return fm != nullptr && inbox.send(message);
}

bool sendPacket(std::span<const std::uint8_t> frame, const PacketChannel &channel)
{
    if (fm == nullptr || modem == nullptr || frame.size() > packetBytes.size() || packetTaken.exchange(true)) {
        return false;
    }
    packetChannel = channel;
    for (std::size_t i = 0; i < frame.size(); ++i) {
        packetBytes[i] = frame[i];
    }
    packetLength = frame.size();
    if (!inbox.send(SendPacket{})) {
        packetTaken = false;
        return false;
    }
    return true;
}

} // namespace dmm::app::radio
