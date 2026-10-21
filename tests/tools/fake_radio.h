// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tommi Tauriainen

#pragma once

#include "transport.h"

#include <QMap>

/// In-memory radio speaking the read side of the programming protocol,
/// including the A/B copy selection the radio applies to reads.
class FakeRadio final : public Transport
{
public:
    static constexpr quint32 BlockSize = 0x20000;

    QByteArray model = QByteArrayLiteral("D878UV2");
    int corruptReplies = 0; // the next n read replies get a bad checksum
    int dropReplies = 0;    // the next n read requests get no reply
    bool ended = false;
    int readRequests = 0;

    void setBytes(quint32 address, const QByteArray &bytes)
    {
        for (qsizetype i = 0; i < bytes.size(); ++i) {
            m_memory[address + static_cast<quint32>(i)] = bytes.at(i);
        }
    }

    char byteAt(quint32 address) const
    {
        return m_memory.value(address, '\xFF');
    }

    /// Marks the 128 KB block at @p block valid or superseded.
    void markBlock(quint32 block, bool valid)
    {
        setBytes(block + BlockSize - 8, valid ? QByteArray("\xFF\xFF\xFF\xFF\x55\x55\xAA\xAA", 8) : QByteArray("\xAA\xAA\x55\x55\x55\x55\xAA\xAA", 8));
    }

    bool blockValid(quint32 block) const
    {
        auto word = [this](quint32 a) {
            QByteArray w;
            for (quint32 i = 0; i < 4; ++i) {
                w.append(byteAt(a + i));
            }
            return w;
        };
        return word(block + BlockSize - 4) == QByteArray("\x55\x55\xAA\xAA", 4) &&
            word(block + BlockSize - 8) != QByteArray("\xAA\xAA\x55\x55", 4);
    }

    bool write(const QByteArray &data) override
    {
        if (data == "PROGRAM") {
            m_pending.append("QX\x06", 3);
        } else if (data == QByteArray(1, '\x02')) {
            QByteArray reply(1, 'I');
            reply.append(model.leftJustified(7, '\0', true));
            reply.append('\x03');
            reply.append(QByteArray("V100\0\0", 6));
            reply.append('\x06');
            m_pending.append(reply);
        } else if (data.size() == 6 && data.at(0) == 'R') {
            ++readRequests;
            if (dropReplies > 0) {
                --dropReplies;
                return true;
            }
            quint32 address = (quint32(quint8(data.at(1))) << 24) | (quint32(quint8(data.at(2))) << 16) |
                (quint32(quint8(data.at(3))) << 8) | quint32(quint8(data.at(4)));
            const int length = quint8(data.at(5));
            const quint32 requested = address;
            if (!blockValid(address & ~(BlockSize - 1))) {
                address += BlockSize;
            }
            QByteArray reply(1, 'W');
            reply.append(data.mid(1, 5));
            for (int i = 0; i < length; ++i) {
                reply.append(byteAt(address + static_cast<quint32>(i)));
            }
            quint8 sum = 0;
            for (qsizetype i = 1; i < reply.size(); ++i) {
                sum = static_cast<quint8>(sum + quint8(reply.at(i)));
            }
            if (corruptReplies > 0) {
                --corruptReplies;
                ++sum;
            }
            reply.append(static_cast<char>(sum));
            reply.append('\x06');
            Q_UNUSED(requested);
            m_pending.append(reply);
        } else if (data == "END") {
            ended = true;
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
