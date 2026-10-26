// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Henri Lavikainen

#include "update_client.h"

#include <QtEndian>

namespace {

constexpr char Ack = 0x06;
constexpr char FrameStart = 0x01;
constexpr char Identify = 0x02;
constexpr char Finish = 0x18;
constexpr int FrameSize = 1 + 4 + UpdateClient::FrameDataSize + 2 + 1;
constexpr int ReplyTimeoutMs = 1000;
// A frame that starts a flash page waits for its erase, about 80 ms.
constexpr int FrameTimeoutMs = 2000;

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

UpdateClient::UpdateClient(Transport &transport)
    : m_transport(transport)
{
}

bool UpdateClient::start(QString *error)
{
    m_transport.discardInput();
    if (!m_transport.write(QByteArrayLiteral("UPDATE"))) {
        setError(error, QStringLiteral("cannot write to the radio"));
        return false;
    }
    return expectAck(ReplyTimeoutMs, QStringLiteral("the start of the update"), error);
}

std::optional<BootIdentity> UpdateClient::identify(QString *error)
{
    if (!m_transport.write(QByteArray(1, Identify))) {
        setError(error, QStringLiteral("cannot write to the radio"));
        return std::nullopt;
    }
    // 'I', model (8, zero-padded), version (6, zero-padded), ACK.
    const QByteArray reply = m_transport.read(16, ReplyTimeoutMs);
    if (reply.size() != 16 || reply.at(0) != 'I' || reply.at(15) != Ack) {
        setError(error, QStringLiteral("unexpected identify reply %1").arg(QString::fromLatin1(reply.toHex(' '))));
        return std::nullopt;
    }
    auto text = [](const QByteArray &bytes) {
        const qsizetype end = bytes.indexOf('\0');
        return QString::fromLatin1(end < 0 ? bytes : bytes.left(end)).trimmed();
    };
    return BootIdentity{text(reply.mid(1, 8)), text(reply.mid(9, 6))};
}

bool UpdateClient::write(quint32 address, const QByteArray &image, QString *error, const Progress &progress)
{
    for (qsizetype offset = 0; offset < image.size(); offset += FrameDataSize) {
        const quint32 at = address + static_cast<quint32>(offset);
        const QByteArray data = image.mid(offset, FrameDataSize).leftJustified(FrameDataSize, '\0');
        if (!m_transport.write(frame(at, data))) {
            setError(error, QStringLiteral("cannot write to the radio"));
            return false;
        }
        if (!expectAck(FrameTimeoutMs, QStringLiteral("the write at %1").arg(hexAddress(at)), error)) {
            return false;
        }
        if (progress) {
            progress(qMin<qint64>(offset + FrameDataSize, image.size()), image.size());
        }
    }
    return true;
}

bool UpdateClient::finish(QString *error)
{
    if (!m_transport.write(QByteArray(1, Finish))) {
        setError(error, QStringLiteral("cannot write to the radio"));
        return false;
    }
    return expectAck(FrameTimeoutMs, QStringLiteral("the end of the update"), error);
}

QByteArray UpdateClient::frame(quint32 address, const QByteArray &data)
{
    QByteArray out(FrameSize, '\0');
    out[0] = FrameStart;
    qToLittleEndian(address, out.data() + 1);
    out.replace(5, FrameDataSize, data);
    quint16 sum = 0;
    for (int i = 1; i < 5 + FrameDataSize; ++i) {
        sum = static_cast<quint16>(sum + static_cast<quint8>(out.at(i)));
    }
    qToLittleEndian(sum, out.data() + 5 + FrameDataSize);
    out[FrameSize - 1] = Ack;
    return out;
}

bool UpdateClient::expectAck(int timeoutMs, const QString &what, QString *error)
{
    const QByteArray reply = m_transport.read(1, timeoutMs);
    if (reply != QByteArray(1, Ack)) {
        setError(error, reply.isEmpty() ? QStringLiteral("no reply to %1").arg(what) : QStringLiteral("radio refused %1 (reply %2)").arg(what, QString::fromLatin1(reply.toHex())));
        return false;
    }
    return true;
}
