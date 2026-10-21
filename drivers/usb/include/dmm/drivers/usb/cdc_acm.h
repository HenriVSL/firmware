// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tommi Tauriainen

#pragma once

#include "dmm/drivers/usb/descriptors.h"
#include "dmm/drivers/usb/device.h"
#include "dmm/lib/ring_buffer.h"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <span>

namespace dmm::drivers::usb {

/// A virtual serial port (CDC-ACM). One task writes and reads it; the
/// buffers are drained and filled from the USB interrupt.
class CdcAcm final : public Function
{
public:
    static constexpr std::size_t TxBufferBytes = 1024;
    static constexpr std::size_t RxBufferBytes = 256;

    CdcAcm(std::uint8_t commInterface, std::uint8_t notifyEndpoint, std::uint8_t outEndpoint, std::uint8_t inEndpoint);

    /// True while the device is configured and a program on the host has
    /// the port open (it raised DTR).
    bool isOpen() const { return m_hostOpen.load(std::memory_order_relaxed); }

    /// Queues as much of @p data as fits and returns how much did. Bytes
    /// are dropped while the port is closed, and reported as written.
    std::size_t write(std::span<const std::uint8_t> data);

    /// Bytes free in the send buffer.
    std::size_t writeSpace();

    /// Copies up to @p data.size() received bytes and returns how many.
    std::size_t read(std::span<std::uint8_t> data);

    void onConfigured(hal::UsbDeviceController &controller) override;
    void onDisconnected() override;
    bool ownsInterface(std::uint8_t interface) const override;
    bool ownsEndpoint(std::uint8_t address) const override;
    ControlReply onControl(const SetupPacket &setup, std::span<const std::uint8_t> data) override;
    void onOut(std::uint8_t address) override;
    void onInComplete(std::uint8_t address) override;

private:
    void sendNextPacket();
    void acceptHeldPacket();

    std::uint8_t m_commInterface;
    std::uint8_t m_notifyEndpoint;
    std::uint8_t m_outEndpoint;
    std::uint8_t m_inEndpoint;

    hal::UsbDeviceController *m_controller = nullptr;
    bool m_configured = false;
    bool m_inBusy = false;
    bool m_lastPacketFull = false;
    std::atomic<bool> m_hostOpen{false};
    std::uint8_t m_lineCoding[7] = {0x00, 0xC2, 0x01, 0x00, 0, 0, 8}; // 115200 8N1
    lib::RingBuffer<std::uint8_t, TxBufferBytes> m_tx;
    lib::RingBuffer<std::uint8_t, RxBufferBytes> m_rx;
    std::uint8_t m_packet[CdcDataPacketSize]{};
    std::size_t m_heldBytes = 0;
};

} // namespace dmm::drivers::usb
