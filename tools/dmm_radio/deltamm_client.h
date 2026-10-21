// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tommi Tauriainen

#pragma once

#include "transport.h"

#include <QByteArray>
#include <QList>
#include <QString>

#include <functional>
#include <optional>

/// What a radio running DeltaMM reports about itself and its data flash.
struct DeltaMmInfo
{
    QString version;
    bool storagePresent = false;
    quint8 maker = 0;
    quint8 device = 0;
    quint32 storageBytes = 0;
    /// -1: the radio found no bad-block table.
    int remappedBlocks = -1;
};

/// Client for DeltaMM's data protocol on the radio's data port.
class DeltaMmClient
{
public:
    static constexpr int MaxChunk = 4096;
    using Progress = std::function<void(qint64 done, qint64 total)>;

    explicit DeltaMmClient(Transport &transport);

    std::optional<DeltaMmInfo> info(QString *error, int timeoutMs = 1000);

    struct ReadReport
    {
        int correctedChunks = 0;
        /// Start addresses of chunks the chip flagged as uncorrectable. The
        /// data is what the chip returned and may be wrong.
        QList<quint32> flaggedChunks;
        /// Start addresses of chunks that could not be read (sent as 0xFF).
        QList<quint32> badChunks;
    };

    /// Reads @p length bytes from @p address, logical (remapped) or
    /// physical. False with @p error set when the link fails.
    bool read(quint32 address, quint32 length, bool physical, QByteArray &out, ReadReport &report, QString *error,
        const Progress &progress = {});

private:
    /// Sends @p request and returns the reply's payload and status.
    bool exchange(const QByteArray &request, QByteArray &payload, quint8 &status, QString *error, int timeoutMs);

    Transport &m_transport;
};
