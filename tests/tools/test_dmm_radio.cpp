// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tommi Tauriainen

#include "backup.h"
#include "cps_client.h"
#include "fake_radio.h"
#include "stock_layout.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <doctest/doctest.h>

namespace {

constexpr quint32 CalA = 0x02F60000;
constexpr quint32 CalB = 0x02F80000;
constexpr quint32 InfoA = 0x02FA0000;

QByteArray pattern(int size, int seed)
{
    QByteArray b(size, '\0');
    for (int i = 0; i < size; ++i) {
        b[i] = static_cast<char>((i * 7 + seed) & 0x7F);
    }
    return b;
}

// A radio as it leaves the factory: calibration copy A valid, B
// superseded, radio-info copy A valid.
void setUpRadio(FakeRadio &radio)
{
    QByteArray cal = pattern(0x620, 3);
    cal[0x14] = 'w';
    cal[0x15] = 'w';
    cal[0x108] = 'f';
    radio.setBytes(CalA, cal);
    radio.markBlock(CalA, true);
    radio.setBytes(CalB, pattern(0x620, 9));
    radio.markBlock(CalB, false);

    QByteArray info = pattern(0x800, 5);
    info.replace(0x10, 7, "D878UV2");
    info.replace(0x30, 16, "0123456789ABCDEF");
    radio.setBytes(InfoA, info);
    radio.markBlock(InfoA, true);
}

bool runBackup(FakeRadio &radio, bool quick, Backup &backup, QString *error)
{
    CpsClient client(radio);
    if (!client.enterProgramMode(error)) {
        return false;
    }
    const auto id = client.identify(error);
    if (!id) {
        return false;
    }
    const RadioLayout *layout = layoutForModel(id->model);
    if (!layout) {
        return false;
    }
    BackupOptions options;
    options.quick = quick;
    const bool ok = readBackup(client, *id, *layout, options, backup, error);
    client.leaveProgramMode(nullptr);
    return ok;
}

} // namespace

TEST_CASE("quick backup saves calibration and radio info and verifies")
{
    FakeRadio radio;
    setUpRadio(radio);
    Backup backup;
    QString error;
    REQUIRE(runBackup(radio, true, backup, &error));
    CHECK(radio.ended);
    CHECK(backup.identity.model == "D878UV2");
    CHECK(backup.identity.version == "V100");
    CHECK(backup.verified());
    CHECK(backup.warnings.isEmpty());
    CHECK_FALSE(backup.complete);
    CHECK_FALSE(backup.data.contains("system-area"));
    CHECK(backup.data.value("calibration").size() == 0x620);
    CHECK(backup.serial() == "0123456789ABCDEF");
    CHECK(backup.radioId().size() == 12);
}

TEST_CASE("full backup includes the system area and cross-checks it")
{
    FakeRadio radio;
    setUpRadio(radio);
    Backup backup;
    QString error;
    REQUIRE(runBackup(radio, false, backup, &error));
    CHECK(backup.complete);
    CHECK(backup.data.value("system-area").size() == 0xE0000);
    CHECK(backup.verified());
}

TEST_CASE("reads follow the valid copy")
{
    FakeRadio radio;
    setUpRadio(radio);
    radio.markBlock(CalA, false);
    radio.markBlock(CalB, true);
    CpsClient client(radio);
    QByteArray data;
    REQUIRE(client.read(CalA, 32, data, nullptr));
    CHECK(data == pattern(0x620, 9).left(32));
}

TEST_CASE("a bad checksum or a lost reply is retried")
{
    FakeRadio radio;
    setUpRadio(radio);
    radio.corruptReplies = 2;
    CpsClient client(radio);
    QByteArray data;
    QString error;
    REQUIRE(client.read(CalA, 16, data, &error));
    CHECK(radio.readRequests == 3);

    radio.dropReplies = 3;
    CHECK_FALSE(client.read(CalA, 16, data, &error));
    CHECK(error.contains("timeout"));
}

TEST_CASE("an unmarked calibration block fails verification")
{
    FakeRadio radio;
    setUpRadio(radio);
    radio.setBytes(CalA + FakeRadio::BlockSize - 8, QByteArray(8, '\xFF'));
    radio.markBlock(CalB, false);
    Backup backup;
    QString error;
    REQUIRE(runBackup(radio, true, backup, &error));
    CHECK_FALSE(backup.verified());
    CHECK(backup.problems.join(';').contains("calibration block is marked superseded"));
}

TEST_CASE("a saved backup reloads, and tampering is detected")
{
    FakeRadio radio;
    setUpRadio(radio);
    Backup backup;
    QString error;
    REQUIRE(runBackup(radio, true, backup, &error));

    QTemporaryDir root;
    const QString folder = saveBackup(backup, root.path(), &error);
    REQUIRE_FALSE(folder.isEmpty());
    CHECK(folder.startsWith(root.path() + "/" + backup.radioId() + "/"));

    Backup loaded;
    REQUIRE(loadBackup(folder, loaded, &error));
    CHECK(loaded.verified());
    CHECK(loaded.data == backup.data);

    QFile cal(folder + "/calibration.bin");
    REQUIRE(cal.open(QIODevice::ReadWrite));
    cal.seek(0x40);
    cal.write("X");
    cal.close();
    REQUIRE(loadBackup(folder, loaded, &error));
    CHECK_FALSE(loaded.verified());
    CHECK(loaded.problems.join(';').contains("calibration does not match its recorded hash"));
}

TEST_CASE("flashing needs a verified full backup of the radio")
{
    FakeRadio radio;
    setUpRadio(radio);
    QTemporaryDir root;
    QString error;
    CHECK(findVerifiedBackup(root.path(), {}, &error).isEmpty());
    CHECK(error.contains("no backups"));

    Backup quick;
    REQUIRE(runBackup(radio, true, quick, &error));
    REQUIRE_FALSE(saveBackup(quick, root.path(), &error).isEmpty());
    CHECK(findVerifiedBackup(root.path(), {}, &error).isEmpty());
    CHECK(error.contains("no verified full backup"));

    Backup full;
    REQUIRE(runBackup(radio, false, full, &error));
    full.created = full.created.addSecs(1);
    const QString folder = saveBackup(full, root.path(), &error);
    REQUIRE_FALSE(folder.isEmpty());
    CHECK(findVerifiedBackup(root.path(), {}, &error) == folder);
    CHECK(findVerifiedBackup(root.path(), full.radioId(), &error) == folder);
    CHECK(findVerifiedBackup(root.path(), QStringLiteral("other"), &error).isEmpty());

    QDir(root.path()).mkpath(QStringLiteral("second-radio/20260101-000000"));
    CHECK(findVerifiedBackup(root.path(), {}, &error).isEmpty());
    CHECK(error.contains("--radio"));
}

TEST_CASE("a raw read of the system area reproduces every part of a backup")
{
    // What check-backup relies on: the link's view, rebuilt from raw
    // storage, equals what the backup read over the link.
    FakeRadio radio;
    setUpRadio(radio);
    radio.setBytes(0x02FC0000, pattern(0x800, 11)); // radio info copy B, not used
    Backup backup;
    QString error;
    REQUIRE(runBackup(radio, false, backup, &error));

    constexpr quint32 Base = 0x02F00000;
    QByteArray image(0x100000, '\0');
    for (quint32 i = 0; i < 0x100000; ++i) {
        image[i] = radio.byteAt(Base + i);
    }
    const RadioLayout *layout = layoutForModel("D878UV2");
    REQUIRE(layout != nullptr);
    for (const BackupPart &part : layout->parts) {
        INFO(part.name.toStdString());
        CHECK(stock_layout::linkView(image, Base, part.address, part.length) == backup.data.value(part.name));
    }
}

TEST_CASE("only known models have a layout")
{
    CHECK(layoutForModel("D878UV2") != nullptr);
    CHECK(layoutForModel("D868UVE") == nullptr);
}
