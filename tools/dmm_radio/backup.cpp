// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tommi Tauriainen

#include "backup.h"

#include "dmm/version.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QStandardPaths>

namespace {

// AnyTone D878UVII. The radio keeps its per-unit data in SPI NAND, in
// 128 KB blocks with an A and a B copy 0x20000 apart; the programming
// link always returns whichever copy is marked valid.
//   0x02F60000  RF calibration, about 1.5 KB used
//   0x02F7FFF0  last row of the calibration block: its copy markers
//   0x02FA0000  radio information page: model, band limits, serial number
//   0x02F00000  the whole system area (896 KB) that holds both
const RadioLayout D878Uv2Layout{
    QStringLiteral("D878UV2"),
    {
        {QStringLiteral("radio-info"), 0x02FA0000, 0x800, true, false},
        {QStringLiteral("calibration"), 0x02F60000, 0x620, true, false},
        {QStringLiteral("calibration-markers"), 0x02F7FFF0, 0x10, true, false},
        {QStringLiteral("system-area"), 0x02F00000, 0xE0000, false, true},
    },
};

constexpr qsizetype InfoModelOffset = 0x10;
constexpr qsizetype InfoSerialOffset = 0x30;
constexpr qsizetype InfoSerialLength = 16;

QString sha256Hex(const QByteArray &data)
{
    return QString::fromLatin1(QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex());
}

bool allBytesAre(const QByteArray &data, char value)
{
    for (char c : data) {
        if (c != value) {
            return false;
        }
    }
    return true;
}

const BackupPart *findPart(const RadioLayout &layout, const QString &name)
{
    for (const BackupPart &p : layout.parts) {
        if (p.name == name) {
            return &p;
        }
    }
    return nullptr;
}

} // namespace

const RadioLayout *layoutForModel(const QString &model)
{
    if (model == D878Uv2Layout.model) {
        return &D878Uv2Layout;
    }
    return nullptr;
}

QString Backup::serial() const
{
    const QByteArray info = data.value(QStringLiteral("radio-info"));
    if (info.size() < InfoSerialOffset + InfoSerialLength) {
        return {};
    }
    const QString text = QString::fromLatin1(info.mid(InfoSerialOffset, InfoSerialLength));
    static const QRegularExpression valid(QStringLiteral("^[0-9A-Z]{16}$"));
    return valid.match(text).hasMatch() ? text : QString();
}

QString Backup::radioId() const
{
    const QString s = serial();
    const QByteArray source = s.isEmpty() ? data.value(QStringLiteral("calibration")) : s.toLatin1();
    return sha256Hex(source).left(12);
}

bool readBackup(CpsClient &client, const RadioIdentity &identity, const RadioLayout &layout,
    const BackupOptions &options, Backup &backup, QString *error)
{
    backup = Backup{};
    backup.identity = identity;
    backup.created = QDateTime::currentDateTimeUtc();
    backup.complete = !options.quick;

    for (const BackupPart &part : layout.parts) {
        if (part.fullOnly && options.quick) {
            continue;
        }
        if (options.partStarted) {
            options.partStarted(part.name);
        }
        QByteArray first;
        if (!client.read(part.address, part.length, first, error, options.progress)) {
            return false;
        }
        if (part.readTwice) {
            QByteArray second;
            if (!client.read(part.address, part.length, second, error)) {
                return false;
            }
            if (first != second) {
                backup.problems.append(QStringLiteral("%1 read differently twice").arg(part.name));
            }
        }
        backup.data.insert(part.name, first);
    }
    checkBackup(layout, backup);
    return true;
}

void checkBackup(const RadioLayout &layout, Backup &backup)
{
    // Keep problems found while reading (a mismatch between two reads).
    QStringList problems;
    for (const QString &p : std::as_const(backup.problems)) {
        if (p.endsWith(QStringLiteral("read differently twice"))) {
            problems.append(p);
        }
    }
    QStringList warnings;

    for (const BackupPart &part : layout.parts) {
        if (part.fullOnly && !backup.complete) {
            continue;
        }
        const QByteArray d = backup.data.value(part.name);
        if (d.size() != static_cast<qsizetype>(part.length)) {
            problems.append(QStringLiteral("%1 is missing or has the wrong size").arg(part.name));
        }
    }

    const QByteArray cal = backup.data.value(QStringLiteral("calibration"));
    if (allBytesAre(cal, '\xFF') || allBytesAre(cal, '\0')) {
        problems.append(QStringLiteral("calibration is blank"));
    } else if (cal.size() >= 0x109) {
        if (cal.at(0x14) != 'w' || cal.at(0x15) != 'w') {
            warnings.append(QStringLiteral("calibration first-run marker not found"));
        }
        if (cal.at(0x108) != 'f') {
            warnings.append(QStringLiteral("calibration timing block marker not found"));
        }
    }

    // A copy is valid when its last word is 55 55 AA AA and the word before
    // it is not the "superseded" pattern AA AA 55 55.
    const QByteArray markers = backup.data.value(QStringLiteral("calibration-markers"));
    if (markers.size() == 16) {
        if (markers.mid(12, 4) != QByteArray("\x55\x55\xAA\xAA", 4)) {
            problems.append(QStringLiteral("calibration block is not marked valid"));
        }
        if (markers.mid(8, 4) == QByteArray("\xAA\xAA\x55\x55", 4)) {
            problems.append(QStringLiteral("calibration block is marked superseded"));
        }
    }

    const QByteArray info = backup.data.value(QStringLiteral("radio-info"));
    if (info.size() >= InfoModelOffset + 7 && info.mid(InfoModelOffset, layout.model.size()) != layout.model.toLatin1()) {
        problems.append(QStringLiteral("radio-info does not name the model %1").arg(layout.model));
    }
    if (backup.serial().isEmpty()) {
        warnings.append(QStringLiteral("radio-info has no readable serial number"));
    }

    // The full area contains the other parts; they must agree.
    const BackupPart *area = findPart(layout, QStringLiteral("system-area"));
    if (area && backup.complete && backup.data.contains(area->name)) {
        const QByteArray whole = backup.data.value(area->name);
        for (const BackupPart &part : layout.parts) {
            if (&part == area || part.address < area->address || part.address + part.length > area->address + area->length) {
                continue;
            }
            const QByteArray slice = whole.mid(part.address - area->address, part.length);
            if (slice != backup.data.value(part.name)) {
                problems.append(QStringLiteral("system-area disagrees with %1").arg(part.name));
            }
        }
    }

    backup.problems = problems;
    backup.warnings = warnings;
}

QString defaultBackupRoot()
{
    const QByteArray env = qgetenv("DMM_BACKUP_DIR");
    if (!env.isEmpty()) {
        return QString::fromLocal8Bit(env);
    }
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/deltamm/backups");
}

QString saveBackup(const Backup &backup, const QString &root, QString *error)
{
    const QString folder = QStringLiteral("%1/%2/%3").arg(root, backup.radioId(), backup.created.toString(QStringLiteral("yyyyMMdd-HHmmss")));
    if (!QDir().mkpath(folder)) {
        if (error) {
            *error = QStringLiteral("cannot create %1").arg(folder);
        }
        return {};
    }
    const RadioLayout *layout = layoutForModel(backup.identity.model);
    QJsonArray parts;
    for (auto it = backup.data.cbegin(); it != backup.data.cend(); ++it) {
        const QString file = it.key() + QStringLiteral(".bin");
        QFile f(folder + QLatin1Char('/') + file);
        if (!f.open(QIODevice::WriteOnly) || f.write(it.value()) != it.value().size()) {
            if (error) {
                *error = QStringLiteral("cannot write %1").arg(f.fileName());
            }
            return {};
        }
        const BackupPart *part = layout ? findPart(*layout, it.key()) : nullptr;
        QJsonObject p;
        p[QStringLiteral("name")] = it.key();
        p[QStringLiteral("file")] = file;
        p[QStringLiteral("address")] = QStringLiteral("0x%1").arg(part ? part->address : 0, 8, 16, QLatin1Char('0'));
        p[QStringLiteral("length")] = static_cast<qint64>(it.value().size());
        p[QStringLiteral("sha256")] = sha256Hex(it.value());
        parts.append(p);
    }
    QJsonObject m;
    m[QStringLiteral("tool")] = QStringLiteral("dmm-radio");
    m[QStringLiteral("toolVersion")] = QString::fromLatin1(dmm::Version);
    m[QStringLiteral("created")] = backup.created.toString(Qt::ISODate);
    m[QStringLiteral("model")] = backup.identity.model;
    m[QStringLiteral("radioVersion")] = backup.identity.version;
    m[QStringLiteral("bands")] = backup.identity.bands;
    m[QStringLiteral("radioId")] = backup.radioId();
    m[QStringLiteral("serial")] = backup.serial();
    m[QStringLiteral("complete")] = backup.complete;
    m[QStringLiteral("verified")] = backup.verified();
    m[QStringLiteral("problems")] = QJsonArray::fromStringList(backup.problems);
    m[QStringLiteral("warnings")] = QJsonArray::fromStringList(backup.warnings);
    m[QStringLiteral("parts")] = parts;
    QFile manifest(folder + QStringLiteral("/manifest.json"));
    if (!manifest.open(QIODevice::WriteOnly) || manifest.write(QJsonDocument(m).toJson()) < 0) {
        if (error) {
            *error = QStringLiteral("cannot write %1").arg(manifest.fileName());
        }
        return {};
    }
    return folder;
}

bool loadBackup(const QString &folder, Backup &backup, QString *error)
{
    QFile manifest(folder + QStringLiteral("/manifest.json"));
    if (!manifest.open(QIODevice::ReadOnly)) {
        if (error) {
            *error = QStringLiteral("no manifest in %1").arg(folder);
        }
        return false;
    }
    const QJsonObject m = QJsonDocument::fromJson(manifest.readAll()).object();
    backup = Backup{};
    backup.identity.model = m.value(QStringLiteral("model")).toString();
    backup.identity.version = m.value(QStringLiteral("radioVersion")).toString();
    backup.identity.bands = static_cast<quint8>(m.value(QStringLiteral("bands")).toInt());
    backup.created = QDateTime::fromString(m.value(QStringLiteral("created")).toString(), Qt::ISODate);
    backup.complete = m.value(QStringLiteral("complete")).toBool();

    QStringList hashProblems;
    const QJsonArray parts = m.value(QStringLiteral("parts")).toArray();
    for (const QJsonValue &v : parts) {
        const QJsonObject p = v.toObject();
        QFile f(folder + QLatin1Char('/') + p.value(QStringLiteral("file")).toString());
        if (!f.open(QIODevice::ReadOnly)) {
            hashProblems.append(QStringLiteral("%1 is missing").arg(p.value(QStringLiteral("name")).toString()));
            continue;
        }
        const QByteArray d = f.readAll();
        if (sha256Hex(d) != p.value(QStringLiteral("sha256")).toString()) {
            hashProblems.append(QStringLiteral("%1 does not match its recorded hash").arg(p.value(QStringLiteral("name")).toString()));
        }
        backup.data.insert(p.value(QStringLiteral("name")).toString(), d);
    }
    const RadioLayout *layout = layoutForModel(backup.identity.model);
    if (!layout) {
        if (error) {
            *error = QStringLiteral("unsupported model %1").arg(backup.identity.model);
        }
        return false;
    }
    checkBackup(*layout, backup);
    backup.problems = hashProblems + backup.problems;
    return true;
}

QString findVerifiedBackup(const QString &root, const QString &radioId, QString *error)
{
    const QDir dir(root);
    QString radio = radioId;
    if (radio.isEmpty()) {
        const QStringList radios = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
        if (radios.isEmpty()) {
            *error = QStringLiteral("no backups found in %1").arg(QDir::toNativeSeparators(root));
            return {};
        }
        if (radios.size() > 1) {
            *error = QStringLiteral("backups of several radios found (%1); choose one with --radio").arg(radios.join(QStringLiteral(", ")));
            return {};
        }
        radio = radios.constFirst();
    }
    const QDir radioDir(dir.filePath(radio));
    QStringList times = radioDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for (auto it = times.crbegin(); it != times.crend(); ++it) {
        const QString folder = radioDir.filePath(*it);
        Backup backup;
        if (loadBackup(folder, backup, nullptr) && backup.verified() && backup.complete) {
            return folder;
        }
    }
    *error = QStringLiteral("no verified full backup of radio %1 in %2").arg(radio, QDir::toNativeSeparators(root));
    return {};
}
