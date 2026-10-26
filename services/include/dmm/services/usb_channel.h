// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tommi Tauriainen

#pragma once

#include "dmm/drivers/usb/cdc_acm.h"
#include "dmm/services/byte_channel.h"

namespace dmm::services {

/// A USB serial port as a byte channel.
class UsbChannel
{
public:
    explicit UsbChannel(drivers::usb::CdcAcm &port)
        : m_port(port)
    {
    }

    bool isOpen() { return m_port.isOpen(); }
    std::size_t write(std::span<const std::uint8_t> data) { return m_port.write(data); }
    std::size_t read(std::span<std::uint8_t> data) { return m_port.read(data); }

private:
    drivers::usb::CdcAcm &m_port;
};

static_assert(ByteChannel<UsbChannel>);

} // namespace dmm::services
