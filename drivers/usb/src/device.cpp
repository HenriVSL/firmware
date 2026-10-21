// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tommi Tauriainen

#include "dmm/drivers/usb/device.h"

#include "dmm/drivers/usb/descriptors.h"

namespace dmm::drivers::usb {

namespace {

constexpr std::uint8_t Ep0Out = 0x00;
constexpr std::uint8_t Ep0In = 0x80;

namespace request {
constexpr std::uint8_t GetStatus = 0;
constexpr std::uint8_t ClearFeature = 1;
constexpr std::uint8_t SetFeature = 3;
constexpr std::uint8_t SetAddress = 5;
constexpr std::uint8_t GetDescriptor = 6;
constexpr std::uint8_t GetConfiguration = 8;
constexpr std::uint8_t SetConfiguration = 9;
constexpr std::uint8_t GetInterface = 10;
constexpr std::uint8_t SetInterface = 11;
} // namespace request

constexpr unsigned RecipientInterface = 1;
constexpr unsigned TypeStandard = 0;

ControlReply accepted()
{
    return {true, nullptr, 0};
}

ControlReply reply(std::span<const std::uint8_t> bytes)
{
    return {true, bytes.data(), bytes.size()};
}

} // namespace

SetupPacket SetupPacket::parse(const std::uint8_t (&raw)[8])
{
    const auto word = [&](int at) { return static_cast<std::uint16_t>(raw[at] | (raw[at + 1] << 8)); };
    return {raw[0], raw[1], word(2), word(4), word(6)};
}

Device::Device(hal::UsbDeviceController &controller, const Descriptors &descriptors, std::span<Function *const> functions)
    : m_controller(controller), m_descriptors(descriptors), m_functions(functions)
{
}

void Device::start()
{
    m_controller.start(*this);
}

void Device::disconnectFunctions()
{
    m_configured.store(false, std::memory_order_relaxed);
    for (Function *f : m_functions) {
        f->onDisconnected();
    }
}

void Device::onBusReset()
{
    m_stage = Stage::Idle;
    m_pendingAddress = 0;
    m_configuration = 0;
    disconnectFunctions();
}

// A suspended bus keeps its configuration: after resume the host carries
// on without a new reset. Pending IN data simply waits.
void Device::onSuspend() {}

void Device::onSetup(const std::uint8_t (&packet)[8])
{
    m_setup = SetupPacket::parse(packet);
    m_inRemaining = 0;
    m_inNeedsEmptyPacket = false;
    if (!m_setup.deviceToHost() && m_setup.length > 0) {
        if (m_setup.length > MaxOutData) {
            stallControl();
            return;
        }
        m_stage = Stage::DataOut;
        m_outReceived = 0;
        m_controller.armOut(Ep0Out);
        return;
    }
    dispatch(m_setup, {});
}

void Device::dispatch(const SetupPacket &setup, std::span<const std::uint8_t> data)
{
    ControlReply r;
    if (setup.type() == TypeStandard) {
        r = standardRequest(setup);
    } else if (setup.recipient() == RecipientInterface) {
        Function *f = functionForInterface(static_cast<std::uint8_t>(setup.index));
        if (f != nullptr) {
            r = f->onControl(setup, data);
        }
    }
    if (!r.accepted) {
        stallControl();
        return;
    }
    if (setup.deviceToHost()) {
        m_inData = r.data;
        m_inRemaining = r.length < setup.length ? r.length : setup.length;
        // A reply shorter than asked for ends with a short packet.
        m_inNeedsEmptyPacket = m_inRemaining < setup.length && m_inRemaining % Ep0PacketSize == 0;
        m_stage = Stage::DataIn;
        m_controller.armOut(Ep0Out); // the host may end the data stage early
        sendNextIn();
    } else {
        m_stage = Stage::StatusIn;
        m_controller.write(Ep0In, nullptr, 0);
    }
}

void Device::sendNextIn()
{
    const std::size_t n = m_inRemaining < Ep0PacketSize ? m_inRemaining : Ep0PacketSize;
    if (n == 0) {
        m_inNeedsEmptyPacket = false;
    }
    m_controller.write(Ep0In, m_inData, n);
    m_inData += n;
    m_inRemaining -= n;
}

void Device::stallControl()
{
    m_stage = Stage::Idle;
    m_controller.stall(Ep0In);
}

ControlReply Device::standardRequest(const SetupPacket &setup)
{
    switch (setup.request) {
    case request::GetStatus:
        m_reply[0] = setup.recipient() == 0 ? 1 : 0; // self-powered
        m_reply[1] = 0;
        return reply(m_reply);
    case request::ClearFeature:
    case request::SetFeature:
        return accepted();
    case request::SetAddress:
        m_pendingAddress = static_cast<std::uint8_t>(setup.value & 0x7Fu);
        return accepted();
    case request::GetDescriptor:
        return descriptor(setup.value);
    case request::GetConfiguration:
        m_reply[0] = m_configuration;
        return reply(std::span(m_reply, 1));
    case request::SetConfiguration:
        if (setup.value > 1) {
            return {};
        }
        m_controller.closeEndpoints();
        disconnectFunctions();
        m_configuration = static_cast<std::uint8_t>(setup.value);
        if (m_configuration == 1) {
            for (Function *f : m_functions) {
                f->onConfigured(m_controller);
            }
            m_configured.store(true, std::memory_order_relaxed);
        }
        return accepted();
    case request::GetInterface:
        m_reply[0] = 0;
        return reply(std::span(m_reply, 1));
    case request::SetInterface:
        return setup.value == 0 ? accepted() : ControlReply{};
    default:
        return {};
    }
}

ControlReply Device::descriptor(std::uint16_t value) const
{
    const unsigned type = value >> 8;
    const unsigned index = value & 0xFFu;
    switch (type) {
    case 1:
        return reply(m_descriptors.device);
    case 2:
        return reply(m_descriptors.configuration);
    case 3:
        if (index < m_descriptors.strings.size()) {
            return reply(m_descriptors.strings[index]);
        }
        return {};
    default:
        // Full-speed only: no device qualifier or other-speed descriptors.
        return {};
    }
}

Function *Device::functionForInterface(std::uint8_t interface) const
{
    for (Function *f : m_functions) {
        if (f->ownsInterface(interface)) {
            return f;
        }
    }
    return nullptr;
}

Function *Device::functionForEndpoint(std::uint8_t address) const
{
    for (Function *f : m_functions) {
        if (f->ownsEndpoint(address)) {
            return f;
        }
    }
    return nullptr;
}

void Device::onOut(std::uint8_t address)
{
    if (address != Ep0Out) {
        if (Function *f = functionForEndpoint(address)) {
            f->onOut(address);
        }
        return;
    }
    if (m_stage == Stage::DataOut) {
        const std::size_t n = m_controller.read(Ep0Out, m_outData + m_outReceived, MaxOutData - m_outReceived);
        m_outReceived += n;
        if (m_outReceived >= m_setup.length || n < Ep0PacketSize) {
            dispatch(m_setup, std::span<const std::uint8_t>(m_outData, m_outReceived));
        } else {
            m_controller.armOut(Ep0Out);
        }
        return;
    }
    // Status stage of a device-to-host request, possibly ending it early.
    m_stage = Stage::Idle;
    m_controller.armOut(Ep0Out);
}

void Device::onInComplete(std::uint8_t address)
{
    if (address != Ep0In) {
        if (Function *f = functionForEndpoint(address)) {
            f->onInComplete(address);
        }
        return;
    }
    if (m_stage == Stage::DataIn) {
        if (m_inRemaining > 0 || m_inNeedsEmptyPacket) {
            sendNextIn();
        } else {
            m_stage = Stage::StatusOut;
        }
    } else if (m_stage == Stage::StatusIn) {
        if (m_pendingAddress != 0) {
            m_controller.setAddress(m_pendingAddress);
            m_pendingAddress = 0;
        }
        m_stage = Stage::Idle;
        m_controller.armOut(Ep0Out);
    }
}

} // namespace dmm::drivers::usb
