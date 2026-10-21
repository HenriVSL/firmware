// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tommi Tauriainen

#pragma once

#include "dmm/drivers/usb/cdc_acm.h"
#include "dmm/services/log_sink.h"

namespace dmm::services {

/// Sends log lines to a USB serial port, each ended with CR LF. Lines wait
/// in the log queue until a program on the host opens the port.
class UsbLog final : public LogSink
{
public:
    explicit UsbLog(drivers::usb::CdcAcm &port)
        : m_port(port)
    {
    }

    bool ready() override { return m_port.isOpen(); }
    void writeLine(std::string_view line) override;
    std::size_t readInput(std::span<char> buffer) override;

private:
    drivers::usb::CdcAcm &m_port;
};

} // namespace dmm::services
