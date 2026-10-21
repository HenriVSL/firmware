// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tommi Tauriainen

#include "dmm/services/usb_log.h"

#include "dmm/os/os.h"

#include <cstdint>
#include <span>

namespace dmm::services {

namespace {

// How long a line may wait for room before it is dropped, so a host that
// opened the port but stopped reading cannot stall the caller.
constexpr os::Ticks MaxWaitMs = 100;

} // namespace

void UsbLog::writeLine(std::string_view line)
{
    const auto *bytes = reinterpret_cast<const std::uint8_t *>(line.data());
    const std::size_t needed = line.size() + 2;
    os::Ticks waited = 0;
    while (m_port.isOpen() && m_port.writeSpace() < needed) {
        if (waited >= MaxWaitMs || needed > drivers::usb::CdcAcm::TxBufferBytes) {
            return;
        }
        os::sleepMs(1);
        ++waited;
    }
    m_port.write(std::span(bytes, line.size()));
    static constexpr std::uint8_t LineEnd[] = {'\r', '\n'};
    m_port.write(LineEnd);
}

std::size_t UsbLog::readInput(std::span<char> buffer)
{
    return m_port.read(std::span(reinterpret_cast<std::uint8_t *>(buffer.data()), buffer.size()));
}

} // namespace dmm::services
