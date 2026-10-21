// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tommi Tauriainen

#include "stock_layout.h"

namespace stock_layout {

bool blockValid(const QByteArray &image, quint32 imageBase, quint32 block)
{
    const qint64 markers = static_cast<qint64>(block) - imageBase + BlockBytes - 8;
    if (markers < 0 || markers + 8 > image.size()) {
        return false;
    }
    const QByteArray m = image.mid(static_cast<qsizetype>(markers), 8);
    return m.mid(4, 4) == QByteArray("\x55\x55\xAA\xAA", 4) && m.left(4) != QByteArray("\xAA\xAA\x55\x55", 4);
}

QByteArray linkView(const QByteArray &image, quint32 imageBase, quint32 address, quint32 length)
{
    QByteArray out;
    for (quint32 at = address; at < address + length;) {
        const quint32 block = at & ~(BlockBytes - 1);
        const quint32 n = qMin(block + BlockBytes - at, address + length - at);
        const quint32 source = blockValid(image, imageBase, block) ? at : at + BlockBytes;
        const qint64 offset = static_cast<qint64>(source) - imageBase;
        if (offset < 0 || offset + n > image.size()) {
            return {};
        }
        out += image.mid(static_cast<qsizetype>(offset), static_cast<qsizetype>(n));
        at += n;
    }
    return out;
}

} // namespace stock_layout
