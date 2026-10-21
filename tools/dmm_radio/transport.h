// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tommi Tauriainen

#pragma once

#include <QByteArray>
#include <QString>

/// Byte pipe to a radio: a USB serial port, or a fake radio in tests.
class Transport
{
public:
    virtual ~Transport() = default;

    /// Sends @p data. False when the link is gone.
    virtual bool write(const QByteArray &data) = 0;

    /// Returns up to @p count bytes, waiting at most @p timeoutMs for them
    /// all. A short result means a timeout.
    virtual QByteArray read(qsizetype count, int timeoutMs) = 0;

    /// Drops any bytes already received.
    virtual void discardInput() = 0;
};
