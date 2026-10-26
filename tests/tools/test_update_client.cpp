// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Henri Lavikainen

#include "fake_boot_loader.h"
#include "update_client.h"
#include "update_files.h"

#include <doctest/doctest.h>

namespace {

QByteArray image(int size)
{
    QByteArray b(size, '\0');
    for (int i = 0; i < size; ++i) {
        b[i] = static_cast<char>((i * 13 + 5) & 0xFF);
    }
    return b;
}

} // namespace

TEST_CASE("update frame layout")
{
    const QByteArray data = QByteArray::fromHex("80bc002009fd000805fd000805fd000805fd000805fd000805fd000800000000");
    const QByteArray expected = QByteArray::fromHex("010040000880bc002009fd000805fd000805fd000805fd000805fd000805fd000800000000e40706");
    CHECK(UpdateClient::frame(0x08004000, data) == expected);
}

TEST_CASE("update writes the image and finishes")
{
    FakeBootLoader radio;
    UpdateClient client(radio);
    QString error;
    REQUIRE(client.start(&error));
    const auto id = client.identify(&error);
    REQUIRE(id);
    CHECK(id->model == QStringLiteral("D878UV2"));
    CHECK(id->version == QStringLiteral("V100"));

    const QByteArray bytes = image(1000);
    qint64 lastDone = 0;
    const auto progress = [&](qint64 done, qint64 total) {
        CHECK(total == bytes.size());
        lastDone = done;
    };
    REQUIRE(client.write(ImageLoadAddress, bytes, &error, progress));
    REQUIRE(client.finish(&error));

    CHECK(radio.finished);
    CHECK(radio.badChecksums == 0);
    CHECK(radio.frames == 32);
    CHECK(lastDone == bytes.size());
    for (qsizetype i = 0; i < bytes.size(); ++i) {
        REQUIRE(radio.byteAt(ImageLoadAddress + static_cast<quint32>(i)) == bytes.at(i));
    }
    for (qsizetype i = bytes.size(); i < 1024; ++i) {
        REQUIRE(radio.byteAt(ImageLoadAddress + static_cast<quint32>(i)) == '\0');
    }
}

TEST_CASE("update stops at a refused or unanswered frame")
{
    for (const bool silent : {false, true}) {
        FakeBootLoader radio;
        (silent ? radio.silentFrame : radio.refuseFrame) = 3;
        UpdateClient client(radio);
        QString error;
        REQUIRE(client.start(&error));
        REQUIRE(client.identify(&error));
        CHECK_FALSE(client.write(ImageLoadAddress, image(512), &error));
        CHECK(radio.frames == 4);
        CHECK(error.contains(QStringLiteral("0x08004060")));
        CHECK_FALSE(radio.finished);
    }
}

TEST_CASE("update start fails when nothing answers")
{
    class Silent final : public Transport
    {
    public:
        bool write(const QByteArray &) override { return true; }
        QByteArray read(qsizetype, int) override { return {}; }
        void discardInput() override {}
    } silent;
    UpdateClient client(silent);
    QString error;
    CHECK_FALSE(client.start(&error));
    CHECK(error.contains(QStringLiteral("no reply")));
}
