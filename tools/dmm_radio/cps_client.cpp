// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tommi Tauriainen

#include "cps_client.h"

namespace {

constexpr char Ack = 0x06;
constexpr int ReplyTimeoutMs = 1000;
constexpr int RowAttempts = 3;

void setError(QString *error, const QString &text)
{
    if (error) {
        *error = text;
    }
}

QString hexAddress(quint32 address)
{
    return QStringLiteral("0x%1").arg(address, 8, 16, QLatin1Char('0'));
}

} // namespace

CpsClient::CpsClient(Transport &transport)
    : m_transport(transport)
{
}

bool CpsClient::enterProgramMode(QString *error)
{
    m_transport.discardInput();
    if (!m_transport.write(QByteArrayLiteral("PROGRAM"))) {
        setError(error, QStringLiteral("cannot write to the radio"));
        return false;
    }
    const QByteArray reply = m_transport.read(3, ReplyTimeoutMs);
    if (reply != QByteArray("QX\x06", 3)) {
        setError(error, QStringLiteral("radio did not enter programming mode (reply %1)").arg(QString::fromLatin1(reply.toHex(' '))));
        return false;
    }
    return true;
}

std::optional<RadioIdentity> CpsClient::identify(QString *error)
{
    if (!m_transport.write(QByteArray(1, '\x02'))) {
        setError(error, QStringLiteral("cannot write to the radio"));
        return std::nullopt;
    }
    // 'I', model (7), bands (1), version (6), ACK.
    const QByteArray reply = m_transport.read(16, ReplyTimeoutMs);
    if (reply.size() != 16 || reply.at(0) != 'I' || reply.at(15) != Ack) {
        setError(error, QStringLiteral("unexpected identify reply %1").arg(QString::fromLatin1(reply.toHex(' '))));
        return std::nullopt;
    }
    auto text = [](const QByteArray &bytes) {
        const qsizetype end = bytes.indexOf('\0');
        return QString::fromLatin1(end < 0 ? bytes : bytes.left(end)).trimmed();
    };
    RadioIdentity id;
    id.model = text(reply.mid(1, 7));
    id.bands = static_cast<quint8>(reply.at(8));
    id.version = text(reply.mid(9, 6));
    return id;
}

bool CpsClient::readRow(quint32 address, QByteArray &row, QString *error)
{
    QByteArray request(6, '\0');
    request[0] = 'R';
    request[1] = static_cast<char>(address >> 24);
    request[2] = static_cast<char>(address >> 16);
    request[3] = static_cast<char>(address >> 8);
    request[4] = static_cast<char>(address);
    request[5] = static_cast<char>(RowSize);

    QString lastProblem;
    for (int attempt = 0; attempt < RowAttempts; ++attempt) {
        if (attempt > 0) {
            m_transport.discardInput();
        }
        if (!m_transport.write(request)) {
            setError(error, QStringLiteral("cannot write to the radio"));
            return false;
        }
        // 'W', address (4), length (1), data, checksum, ACK.
        const QByteArray reply = m_transport.read(RowSize + 8, ReplyTimeoutMs);
        if (reply.size() != RowSize + 8) {
            lastProblem = QStringLiteral("timeout");
            continue;
        }
        if (reply.at(0) != 'W' || reply.mid(1, 5) != request.mid(1, 5) || reply.at(RowSize + 7) != Ack) {
            lastProblem = QStringLiteral("unexpected reply");
            continue;
        }
        quint8 sum = 0;
        for (int i = 1; i < RowSize + 6; ++i) {
            sum = static_cast<quint8>(sum + static_cast<quint8>(reply.at(i)));
        }
        if (sum != static_cast<quint8>(reply.at(RowSize + 6))) {
            lastProblem = QStringLiteral("bad checksum");
            continue;
        }
        row = reply.mid(6, RowSize);
        return true;
    }
    setError(error, QStringLiteral("read at %1 failed: %2").arg(hexAddress(address), lastProblem));
    return false;
}

bool CpsClient::read(quint32 address, qsizetype length, QByteArray &out, QString *error, const Progress &progress)
{
    if (address % RowSize != 0 || length % RowSize != 0) {
        setError(error, QStringLiteral("read must be aligned to %1 bytes").arg(RowSize));
        return false;
    }
    out.clear();
    out.reserve(length);
    QByteArray row;
    for (qsizetype offset = 0; offset < length; offset += RowSize) {
        if (!readRow(address + static_cast<quint32>(offset), row, error)) {
            return false;
        }
        out.append(row);
        if (progress) {
            progress(offset + RowSize, length);
        }
    }
    return true;
}

bool CpsClient::leaveProgramMode(QString *error)
{
    if (!m_transport.write(QByteArrayLiteral("END"))) {
        setError(error, QStringLiteral("cannot write to the radio"));
        return false;
    }
    const QByteArray reply = m_transport.read(1, ReplyTimeoutMs);
    if (reply != QByteArray(1, Ack)) {
        setError(error, QStringLiteral("radio did not acknowledge the end of the session"));
        return false;
    }
    return true;
}
