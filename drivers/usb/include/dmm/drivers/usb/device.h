// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tommi Tauriainen

#pragma once

#include "dmm/hal/usb_device.h"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <span>

namespace dmm::drivers::usb {

struct SetupPacket
{
    std::uint8_t requestType;
    std::uint8_t request;
    std::uint16_t value;
    std::uint16_t index;
    std::uint16_t length;

    static SetupPacket parse(const std::uint8_t (&raw)[8]);

    bool deviceToHost() const { return (requestType & 0x80u) != 0; }
    /// 0 standard, 1 class, 2 vendor.
    unsigned type() const { return (requestType >> 5) & 3u; }
    /// 0 device, 1 interface, 2 endpoint.
    unsigned recipient() const { return requestType & 0x1Fu; }
};

/// Answer to a control request: accepted or not, and for a device-to-host
/// request the reply bytes (they must stay valid until sent).
struct ControlReply
{
    bool accepted = false;
    const std::uint8_t *data = nullptr;
    std::size_t length = 0;
};

/// One function of a composite device, such as a serial port. Called from
/// the controller's interrupt.
class Function
{
public:
    /// The host selected the configuration: open the endpoints.
    virtual void onConfigured(hal::UsbDeviceController &controller) = 0;
    /// Bus reset, suspend or deconfiguration: the endpoints are gone.
    virtual void onDisconnected() = 0;
    virtual bool ownsInterface(std::uint8_t interface) const = 0;
    virtual bool ownsEndpoint(std::uint8_t address) const = 0;
    /// A class request to one of its interfaces. @p data holds the data
    /// stage of a host-to-device request.
    virtual ControlReply onControl(const SetupPacket &setup, std::span<const std::uint8_t> data) = 0;
    virtual void onOut(std::uint8_t address) = 0;
    virtual void onInComplete(std::uint8_t address) = 0;

protected:
    ~Function() = default;
};

struct Descriptors
{
    std::span<const std::uint8_t> device;
    std::span<const std::uint8_t> configuration;
    /// Index 0 is the language list.
    std::span<const std::span<const std::uint8_t>> strings;
};

/// A full-speed USB device: answers the host's standard requests on
/// endpoint 0 and passes the rest to its functions.
class Device final : public hal::UsbDeviceEvents
{
public:
    Device(hal::UsbDeviceController &controller, const Descriptors &descriptors, std::span<Function *const> functions);

    void start();
    bool configured() const { return m_configured.load(std::memory_order_relaxed); }

    void onBusReset() override;
    void onSetup(const std::uint8_t (&packet)[8]) override;
    void onOut(std::uint8_t address) override;
    void onInComplete(std::uint8_t address) override;
    void onSuspend() override;

private:
    enum class Stage : std::uint8_t
    {
        Idle,
        DataIn,
        DataOut,
        StatusIn,
        StatusOut,
    };

    static constexpr std::size_t MaxOutData = 64;

    void dispatch(const SetupPacket &setup, std::span<const std::uint8_t> data);
    ControlReply standardRequest(const SetupPacket &setup);
    ControlReply descriptor(std::uint16_t value) const;
    Function *functionForInterface(std::uint8_t interface) const;
    Function *functionForEndpoint(std::uint8_t address) const;
    void sendNextIn();
    void stallControl();
    void disconnectFunctions();

    hal::UsbDeviceController &m_controller;
    Descriptors m_descriptors;
    std::span<Function *const> m_functions;

    Stage m_stage = Stage::Idle;
    SetupPacket m_setup{};
    const std::uint8_t *m_inData = nullptr;
    std::size_t m_inRemaining = 0;
    bool m_inNeedsEmptyPacket = false;
    std::uint8_t m_outData[MaxOutData]{};
    std::size_t m_outReceived = 0;
    std::uint8_t m_pendingAddress = 0;
    std::uint8_t m_configuration = 0;
    std::uint8_t m_reply[2]{};
    std::atomic<bool> m_configured{false};
};

} // namespace dmm::drivers::usb
