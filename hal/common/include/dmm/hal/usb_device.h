// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tommi Tauriainen

#pragma once

// USB full-speed device controller, as seen by a portable device stack.
// Endpoint addresses use the USB convention: bit 7 set = IN (to the host).

#include <cstddef>
#include <cstdint>

namespace dmm::hal {

enum class UsbEndpointType : std::uint8_t
{
    Control,
    Bulk,
    Interrupt,
};

/// Bus events, called from the controller's interrupt.
class UsbDeviceEvents
{
public:
    /// The host reset the bus. Endpoint 0 is open again; all others are
    /// closed and the address is 0.
    virtual void onBusReset() = 0;
    /// A SETUP packet arrived on endpoint 0.
    virtual void onSetup(const std::uint8_t (&packet)[8]) = 0;
    /// A packet arrived on OUT endpoint @p address. Read it with read(),
    /// then call armOut() to accept the next one.
    virtual void onOut(std::uint8_t address) = 0;
    /// The packet given to write() on IN endpoint @p address was sent.
    virtual void onInComplete(std::uint8_t address) = 0;
    /// No bus activity for 3 ms: the host suspended the bus or the cable
    /// was pulled.
    virtual void onSuspend() = 0;

protected:
    ~UsbDeviceEvents() = default;
};

class UsbDeviceController
{
public:
    /// Connects to the bus and starts sending events to @p events.
    virtual void start(UsbDeviceEvents &events) = 0;

    /// Takes the address from SET_ADDRESS. Call after its status stage.
    virtual void setAddress(std::uint8_t address) = 0;

    /// Closes every endpoint except 0 and frees their packet memory.
    virtual void closeEndpoints() = 0;

    /// Opens an endpoint other than 0. An OUT endpoint is armed at once.
    virtual void openEndpoint(std::uint8_t address, UsbEndpointType type, std::uint16_t maxPacket) = 0;

    /// Copies the last packet received on OUT endpoint @p address, up to
    /// @p capacity bytes. Returns its length.
    virtual std::size_t read(std::uint8_t address, std::uint8_t *data, std::size_t capacity) = 0;

    /// Lets OUT endpoint @p address accept its next packet.
    virtual void armOut(std::uint8_t address) = 0;

    /// Sends one packet of at most the endpoint's packet size on IN
    /// endpoint @p address; a zero length sends an empty packet.
    virtual void write(std::uint8_t address, const std::uint8_t *data, std::size_t length) = 0;

    /// Answers the next transaction on @p address with STALL (both
    /// directions for endpoint 0, until the next SETUP).
    virtual void stall(std::uint8_t address) = 0;

    /// Holds back controller events, so a task can share state with them.
    virtual void lock() = 0;
    virtual void unlock() = 0;

protected:
    ~UsbDeviceController() = default;
};

/// Holds the controller's events back for its lifetime.
class UsbLockGuard
{
public:
    explicit UsbLockGuard(UsbDeviceController &controller)
        : m_controller(controller)
    {
        m_controller.lock();
    }
    ~UsbLockGuard() { m_controller.unlock(); }
    UsbLockGuard(const UsbLockGuard &) = delete;
    UsbLockGuard &operator=(const UsbLockGuard &) = delete;

private:
    UsbDeviceController &m_controller;
};

} // namespace dmm::hal
