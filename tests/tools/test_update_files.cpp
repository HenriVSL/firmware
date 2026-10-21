// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tommi Tauriainen

#include "update_files.h"

#include <QTemporaryDir>
#include <QtEndian>
#include <doctest/doctest.h>

namespace {

QByteArray le32(quint32 v)
{
    QByteArray b(4, '\0');
    qToLittleEndian(v, b.data());
    return b;
}

// Synthetic factory set in the vendor layout: a 1000-byte image.
UpdateFiles stockSet()
{
    UpdateFiles s;
    s.image = QByteArray(1000, '\x5A');
    s.spi = QByteArray("\x20\0\0\0\x01\0", 6) + le32(1000) + QByteArray(4, '\0') + "D878UV2 V100";
    s.cdi = QByteArray(256, ' ') + le32(0x08004000) + le32(1000) + le32(0) + QByteArray("\0\0\1\0\0\0\0\0\0\0", 10);
    return s;
}

QByteArray imageWith(quint32 stack, quint32 reset, int size)
{
    QByteArray b = le32(stack) + le32(reset);
    b.resize(size, '\0');
    return b;
}

} // namespace

TEST_CASE("factory files are accepted only as a matching D878UVII set")
{
    UpdateFiles s = stockSet();
    CHECK(checkStockFiles(s).isEmpty());

    s.image.chop(1);
    CHECK(checkStockFiles(s).contains("lengths do not match"));

    s = stockSet();
    s.spi.replace(14, 7, "D868UVE");
    CHECK(checkStockFiles(s).contains("not for the D878UVII"));

    s = stockSet();
    s.cdi.replace(256, 4, le32(0x08000000));
    CHECK(checkStockFiles(s).contains("0x08004000"));
}

TEST_CASE("an image needs a RAM stack, a reset handler inside it and room below the radio data")
{
    CHECK(checkImage(imageWith(0x200026D0, 0x080049A1, 9924)).isEmpty());
    CHECK(checkImage(imageWith(0x200026D0, 0x080049A0, 9924)).contains("reset handler"));
    CHECK(checkImage(imageWith(0x200026D0, 0x08010001, 9924)).contains("reset handler"));
    CHECK(checkImage(imageWith(0x08000000, 0x080049A1, 9924)).contains("stack pointer"));
    CHECK(checkImage(imageWith(0x200026D0, 0x080049A1, 0x080F5800 - 0x08004000 + 4)).contains("radio data"));
}

TEST_CASE("update files carry the new length and keep everything else")
{
    const UpdateFiles s = stockSet();
    const QByteArray image = imageWith(0x200026D0, 0x080049A1, 9924);
    const UpdateFiles out = makeUpdateFiles(s, image);
    CHECK(out.image == image);
    CHECK(qFromLittleEndian<quint32>(out.spi.constData() + 6) == 9924);
    CHECK(qFromLittleEndian<quint32>(out.cdi.constData() + 260) == 9924);
    CHECK(out.spi.mid(10) == s.spi.mid(10));
    CHECK(out.cdi.left(260) == s.cdi.left(260));
    CHECK(out.cdi.mid(264) == s.cdi.mid(264));
    CHECK(checkStockFiles(UpdateFiles{out.image, out.spi, out.cdi}).isEmpty());
}
