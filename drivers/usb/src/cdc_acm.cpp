// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tommi Tauriainen

#include "dmm/drivers/usb/cdc_acm.h"

namespace dmm::drivers::usb {

namespace {

namespace request {
constexpr std::uint8_t SetLineCoding = 0x20;
constexpr std::uint8_t GetLineCoding = 0x21;
constexpr std::uint8_t SetControlLineState = 0x22;
constexpr std::uint8_t SendBreak = 0x23;
} // namespace request

constexpr std::uint16_t DtrBit = 1;

} // namespace

CdcAcm::CdcAcm(std::uint8_t commInterface, std::uint8_t notifyEndpoint, std::uint8_t outEndpoint, std::uint8_t inEndpoint)
    : m_commInterface(commInterface), m_notifyEndpoint(notifyEndpoint), m_outEndpoint(outEndpoint), m_inEndpoint(inEndpoint)
{
}

void CdcAcm::onConfigured(hal::UsbDeviceController &controller)
{
    m_controller = &controller;
    controller.openEndpoint(m_notifyEndpoint, hal::UsbEndpointType::Interrupt, CdcNotifyPacketSize);
    controller.openEndpoint(m_outEndpoint, hal::UsbEndpointType::Bulk, CdcDataPacketSize);
    controller.openEndpoint(m_inEndpoint, hal::UsbEndpointType::Bulk, CdcDataPacketSize);
    m_configured = true;
    m_inBusy = false;
    m_lastPacketFull = false;
    m_heldBytes = 0;
}

void CdcAcm::onDisconnected()
{
    m_configured = false;
    m_inBusy = false;
    m_hostOpen.store(false, std::memory_order_relaxed);
    m_tx.clear();
    m_rx.clear();
    m_heldBytes = 0;
}

bool CdcAcm::ownsInterface(std::uint8_t interface) const
{
    return interface == m_commInterface || interface == m_commInterface + 1;
}

bool CdcAcm::ownsEndpoint(std::uint8_t address) const
{
    return address == m_notifyEndpoint || address == m_outEndpoint || address == m_inEndpoint;
}

ControlReply CdcAcm::onControl(const SetupPacket &setup, std::span<const std::uint8_t> data)
{
    switch (setup.request) {
    case request::SetLineCoding:
        for (std::size_t i = 0; i < sizeof m_lineCoding && i < data.size(); ++i) {
            m_lineCoding[i] = data[i];
        }
        return {true, nullptr, 0};
    case request::GetLineCoding:
        return {true, m_lineCoding, sizeof m_lineCoding};
    case request::SetControlLineState: {
        const bool open = m_configured && (setup.value & DtrBit) != 0;
        if (!open) {
            m_tx.clear();
        }
        m_hostOpen.store(open, std::memory_order_relaxed);
        return {true, nullptr, 0};
    }
    case request::SendBreak:
        return {true, nullptr, 0};
    default:
        return {};
    }
}

void CdcAcm::sendNextPacket()
{
    std::size_t n = 0;
    while (n < CdcDataPacketSize && !m_tx.empty()) {
        m_packet[n++] = *m_tx.pop();
    }
    if (n == 0 && !m_lastPacketFull) {
        m_inBusy = false;
        return;
    }
    // A transfer that ends on a full packet needs an empty one after it,
    // or the host keeps waiting for more.
    m_lastPacketFull = n == CdcDataPacketSize;
    m_inBusy = true;
    m_controller->write(m_inEndpoint, m_packet, n);
}

void CdcAcm::onInComplete(std::uint8_t address)
{
    if (address == m_inEndpoint && m_configured) {
        sendNextPacket();
    }
}

void CdcAcm::onOut(std::uint8_t address)
{
    if (address != m_outEndpoint || !m_configured) {
        return;
    }
    std::uint8_t packet[CdcDataPacketSize];
    const std::size_t n = m_controller->read(m_outEndpoint, packet, sizeof packet);
    if (m_rx.freeSpace() < n) {
        // Full: leave the endpoint NAKing; read() accepts it later.
        m_heldBytes = n;
        return;
    }
    for (std::size_t i = 0; i < n; ++i) {
        m_rx.push(packet[i]);
    }
    m_controller->armOut(m_outEndpoint);
}

void CdcAcm::acceptHeldPacket()
{
    if (m_heldBytes == 0 || m_rx.freeSpace() < m_heldBytes) {
        return;
    }
    std::uint8_t packet[CdcDataPacketSize];
    const std::size_t n = m_controller->read(m_outEndpoint, packet, sizeof packet);
    for (std::size_t i = 0; i < n; ++i) {
        m_rx.push(packet[i]);
    }
    m_heldBytes = 0;
    m_controller->armOut(m_outEndpoint);
}

std::size_t CdcAcm::write(std::span<const std::uint8_t> data)
{
    if (!isOpen()) {
        return data.size();
    }
    hal::UsbLockGuard guard(*m_controller);
    if (!m_configured || !m_hostOpen.load(std::memory_order_relaxed)) {
        return data.size();
    }
    std::size_t n = 0;
    while (n < data.size() && m_tx.push(data[n])) {
        ++n;
    }
    if (!m_inBusy) {
        sendNextPacket();
    }
    return n;
}

std::size_t CdcAcm::writeSpace()
{
    if (!isOpen()) {
        return m_tx.capacity();
    }
    hal::UsbLockGuard guard(*m_controller);
    return m_tx.freeSpace();
}

std::size_t CdcAcm::read(std::span<std::uint8_t> data)
{
    if (m_controller == nullptr) {
        return 0;
    }
    hal::UsbLockGuard guard(*m_controller);
    std::size_t n = 0;
    while (n < data.size() && !m_rx.empty()) {
        data[n++] = *m_rx.pop();
    }
    if (m_configured) {
        acceptHeldPacket();
    }
    return n;
}

} // namespace dmm::drivers::usb
