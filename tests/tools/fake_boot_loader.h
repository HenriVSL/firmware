// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Henri Lavikainen

#pragma once

#include "transport.h"

#include <QMap>
#include <QtEndian>

/// In-memory radio in update mode: answers the start and identify
/// requests, checks and stores write frames, and acknowledges the finish.
class FakeBootLoader final : public Transport
{
public:
    QByteArray model = QByteArrayLiteral("D878UV2");
    bool started = false;
    bool finished = false;
    int frames = 0;
    int refuseFrame = -1; // this frame (0-based) gets a NAK instead of an ACK
    int silentFrame = -1; // this frame gets no reply
    int badChecksums = 0;

    char byteAt(quint32 address) const
    {
        return m_memory.value(address, '\xFF');
    }

    bool write(const QByteArray &data) override
    {
        if (data == "UPDATE") {
            started = true;
            m_pending.append('\x06');
        } else if (data == QByteArray(1, '\x02') && started) {
            QByteArray reply(1, 'I');
            reply.append(model.leftJustified(8, '\0', true));
            reply.append(QByteArray("V100\0\0", 6));
            reply.append('\x06');
            m_pending.append(reply);
        } else if (data.size() == 40 && data.at(0) == '\x01' && started) {
            const int index = frames++;
            quint16 sum = 0;
            for (int i = 1; i < 37; ++i) {
                sum = static_cast<quint16>(sum + quint8(data.at(i)));
            }
            if (sum != qFromLittleEndian<quint16>(data.constData() + 37) || data.at(39) != '\x06') {
                ++badChecksums;
                m_pending.append('\x15');
                return true;
            }
            if (index == silentFrame) {
                return true;
            }
            if (index == refuseFrame) {
                m_pending.append('\x15');
                return true;
            }
            const quint32 address = qFromLittleEndian<quint32>(data.constData() + 1);
            for (int i = 0; i < 32; ++i) {
                m_memory[address + static_cast<quint32>(i)] = data.at(5 + i);
            }
            m_pending.append('\x06');
        } else if (data == QByteArray(1, '\x18') && started) {
            finished = true;
            m_pending.append('\x06');
        }
        return true;
    }

    QByteArray read(qsizetype count, int) override
    {
        const QByteArray out = m_pending.left(count);
        m_pending.remove(0, out.size());
        return out;
    }

    void discardInput() override
    {
        m_pending.clear();
    }

private:
    QMap<quint32, char> m_memory;
    QByteArray m_pending;
};
