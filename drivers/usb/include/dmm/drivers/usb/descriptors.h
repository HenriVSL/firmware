// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tommi Tauriainen

#pragma once

// Compile-time builders for USB descriptors (USB 2.0 chapter 9, CDC 1.2).

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>

namespace dmm::drivers::usb {

using Byte = std::uint8_t;

constexpr Byte lowByte(unsigned v)
{
    return static_cast<Byte>(v & 0xFFu);
}

constexpr Byte highByte(unsigned v)
{
    return static_cast<Byte>((v >> 8) & 0xFFu);
}

/// Joins descriptors into one block.
template <std::size_t... N>
constexpr std::array<Byte, (N + ...)> join(const std::array<Byte, N> &...parts)
{
    std::array<Byte, (N + ...)> out{};
    std::size_t at = 0;
    ((std::copy(parts.begin(), parts.end(), out.begin() + static_cast<std::ptrdiff_t>(at)), at += N), ...);
    return out;
}

struct DeviceInfo
{
    std::uint16_t vendorId;
    std::uint16_t productId;
    std::uint16_t release; // BCD
    Byte manufacturerString;
    Byte productString;
    Byte serialString;
};

inline constexpr std::uint16_t Ep0PacketSize = 64;

/// Device descriptor of a composite device whose functions are grouped by
/// interface association descriptors.
constexpr std::array<Byte, 18> compositeDeviceDescriptor(const DeviceInfo &d)
{
    return {18, 1, 0x00, 0x02, 0xEF, 0x02, 0x01, Ep0PacketSize, lowByte(d.vendorId), highByte(d.vendorId),
        lowByte(d.productId), highByte(d.productId), lowByte(d.release), highByte(d.release), d.manufacturerString,
        d.productString, d.serialString, 1};
}

/// Configuration descriptor header for a self-powered device.
constexpr std::array<Byte, 9> configurationHeader(std::uint16_t totalLength, Byte interfaces, Byte maxPower2mA)
{
    return {9, 2, lowByte(totalLength), highByte(totalLength), interfaces, 1, 0, 0xC0, maxPower2mA};
}

/// String descriptor 0: US English only.
constexpr std::array<Byte, 4> languageDescriptor()
{
    return {4, 3, 0x09, 0x04};
}

/// String descriptor from ASCII text.
template <std::size_t N>
constexpr std::array<Byte, 2 * N> stringDescriptor(const char (&text)[N])
{
    std::array<Byte, 2 * N> out{};
    out[0] = static_cast<Byte>(2 * N);
    out[1] = 3;
    for (std::size_t i = 0; i + 1 < N; ++i) {
        out[2 + 2 * i] = static_cast<Byte>(text[i]);
    }
    return out;
}

inline constexpr std::size_t CdcAcmDescriptorBytes = 66;
inline constexpr std::uint16_t CdcNotifyPacketSize = 16;
inline constexpr std::uint16_t CdcDataPacketSize = 64;

/// The descriptors of one CDC-ACM serial port: an interface association,
/// the communication interface with its notification endpoint, and the
/// data interface (@p commInterface + 1) with its bulk endpoints.
constexpr std::array<Byte, CdcAcmDescriptorBytes> cdcAcmDescriptors(Byte commInterface, Byte notifyEndpoint,
    Byte outEndpoint, Byte inEndpoint, Byte functionString)
{
    const Byte dataInterface = static_cast<Byte>(commInterface + 1);
    return {
        // Interface association: two interfaces, CDC ACM.
        8, 0x0B, commInterface, 2, 0x02, 0x02, 0x01, functionString,
        // Communication interface, one endpoint, CDC ACM, AT commands.
        9, 0x04, commInterface, 0, 1, 0x02, 0x02, 0x01, functionString,
        // Header functional descriptor, CDC 1.10.
        5, 0x24, 0x00, 0x10, 0x01,
        // Call management: handled by the device, data interface named.
        5, 0x24, 0x01, 0x00, dataInterface,
        // Abstract control management: line coding and control line state.
        4, 0x24, 0x02, 0x02,
        // Union: communication interface controls the data interface.
        5, 0x24, 0x06, commInterface, dataInterface,
        // Notification endpoint, interrupt, every 16 ms.
        7, 0x05, notifyEndpoint, 0x03, lowByte(CdcNotifyPacketSize), highByte(CdcNotifyPacketSize), 16,
        // Data interface, two endpoints, CDC data.
        9, 0x04, dataInterface, 0, 2, 0x0A, 0x00, 0x00, 0,
        // Bulk OUT and IN.
        7, 0x05, outEndpoint, 0x02, lowByte(CdcDataPacketSize), highByte(CdcDataPacketSize), 0,
        7, 0x05, inEndpoint, 0x02, lowByte(CdcDataPacketSize), highByte(CdcDataPacketSize), 0};
}

} // namespace dmm::drivers::usb
