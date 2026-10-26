// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Henri Lavikainen

#pragma once

#include "transport.h"

#include <QByteArray>
#include <QString>

#include <functional>
#include <optional>

/// What the boot loader reports about the radio in update mode.
struct BootIdentity
{
    QString model;
    QString version;
};

/// Client for the radio's boot loader in update mode (power on holding PTT
/// and PF3, red LED blinking): start, identify, write the image in
/// 32-byte frames, finish. The boot loader erases each flash page when the
/// first frame for it arrives, and starts the new image after the finish.
class UpdateClient
{
public:
    /// Image bytes per frame.
    static constexpr int FrameDataSize = 32;

    using Progress = std::function<void(qint64 done, qint64 total)>;

    explicit UpdateClient(Transport &transport);

    bool start(QString *error);
    std::optional<BootIdentity> identify(QString *error);

    /// Writes @p image to flash from @p address on, padding the last frame
    /// with zeros. Stops at the first frame the radio does not acknowledge.
    bool write(quint32 address, const QByteArray &image, QString *error, const Progress &progress = {});

    /// Ends the session; the radio acknowledges and starts the new image.
    bool finish(QString *error);

    /// One write frame: 0x01, the address (little-endian), the data, the
    /// 16-bit sum of address and data bytes (little-endian), 0x06.
    /// @p data must be FrameDataSize bytes.
    static QByteArray frame(quint32 address, const QByteArray &data);

private:
    bool expectAck(int timeoutMs, const QString &what, QString *error);

    Transport &m_transport;
};
