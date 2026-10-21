// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tommi Tauriainen

#pragma once

#include "transport.h"

#include <QByteArray>
#include <QString>

#include <functional>
#include <optional>

/// What a radio reports about itself in programming mode.
struct RadioIdentity
{
    QString model;
    quint8 bands = 0;
    QString version;
};

/// Client for the radio's CPS programming protocol, read side only:
/// enter programming mode, identify, read memory, leave.
class CpsClient
{
public:
    /// Bytes per read request; what the radio is known to answer reliably.
    static constexpr int RowSize = 16;

    using Progress = std::function<void(qint64 done, qint64 total)>;

    explicit CpsClient(Transport &transport);

    bool enterProgramMode(QString *error);
    std::optional<RadioIdentity> identify(QString *error);

    /// Reads @p length bytes from radio memory at @p address into @p out.
    /// @p address and @p length must be multiples of RowSize. Each row is
    /// retried a few times on a timeout or a bad checksum.
    bool read(quint32 address, qsizetype length, QByteArray &out, QString *error, const Progress &progress = {});

    /// Ends the session. The radio acknowledges, then restarts.
    bool leaveProgramMode(QString *error);

private:
    bool readRow(quint32 address, QByteArray &row, QString *error);

    Transport &m_transport;
};
