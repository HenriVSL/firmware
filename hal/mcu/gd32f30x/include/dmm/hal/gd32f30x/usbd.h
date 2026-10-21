// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tommi Tauriainen

#pragma once

#include "dmm/hal/gd32f30x/gpio.h"
#include "dmm/hal/usb_device.h"

#include <cstdint>

namespace dmm::hal::gd32f30x {

/// The GD32F30x USB full-speed device block (USBD) on PA11/PA12, clocked
/// from the PLL. Single-buffered endpoints 0..7, endpoint number n in
/// register n. The board calls handleInterrupt() from the USBD low-priority
/// interrupt. lock() and unlock() are for one task at a time.
class Usbd final : public UsbDeviceController
{
public:
    /// @p pullUp drives the D+ pull-up resistor; high = attached.
    Usbd(Pin pullUp, unsigned irqPriority);

    void start(UsbDeviceEvents &events) override;
    void setAddress(std::uint8_t address) override;
    void closeEndpoints() override;
    void openEndpoint(std::uint8_t address, UsbEndpointType type, std::uint16_t maxPacket) override;
    std::size_t read(std::uint8_t address, std::uint8_t *data, std::size_t capacity) override;
    void armOut(std::uint8_t address) override;
    void write(std::uint8_t address, const std::uint8_t *data, std::size_t length) override;
    void stall(std::uint8_t address) override;
    void lock() override;
    void unlock() override;

    void handleInterrupt();

private:
    static constexpr unsigned EndpointCount = 8;

    void resetEndpoints();
    std::uint16_t allocate(std::uint16_t bytes);

    Pin m_pullUp;
    unsigned m_irqPriority;
    UsbDeviceEvents *m_events = nullptr;
    std::uint16_t m_nextFree = 0;
    std::uint16_t m_txBuffer[EndpointCount]{};
    std::uint16_t m_rxBuffer[EndpointCount]{};
    unsigned m_lockDepth = 0;
};

} // namespace dmm::hal::gd32f30x
