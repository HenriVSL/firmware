// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tommi Tauriainen

#pragma once

#include "cps_client.h"

#include <QByteArray>
#include <QDateTime>
#include <QList>
#include <QMap>
#include <QString>
#include <QStringList>

/// One memory range saved by a backup.
struct BackupPart
{
    QString name;
    quint32 address = 0;
    quint32 length = 0;
    /// Read twice and compared; for the small, irreplaceable parts.
    bool readTwice = false;
    /// Left out of quick backups.
    bool fullOnly = false;
};

/// Where a radio model keeps the data worth saving.
struct RadioLayout
{
    QString model;
    QList<BackupPart> parts;
};

/// The layout for @p model, or nullptr when the model is not supported.
const RadioLayout *layoutForModel(const QString &model);

/// A finished backup in memory: data per part plus what the checks found.
struct Backup
{
    RadioIdentity identity;
    QDateTime created;
    QMap<QString, QByteArray> data;
    QStringList problems;
    QStringList warnings;
    bool complete = false;

    /// True when every check passed; only then may a radio be flashed.
    bool verified() const
    {
        return problems.isEmpty() && !data.isEmpty();
    }

    /// Short, stable, non-identifying id: a hash of the radio's serial number.
    QString radioId() const;
    /// The serial number from the radio-info page, empty when unreadable.
    QString serial() const;
};

struct BackupOptions
{
    bool quick = false;
    CpsClient::Progress progress;
    std::function<void(const QString &part)> partStarted;
};

/// Reads every part of @p layout over an open programming session and
/// runs the checks. Returns false, with @p error set, when a read fails.
bool readBackup(CpsClient &client, const RadioIdentity &identity, const RadioLayout &layout,
    const BackupOptions &options, Backup &backup, QString *error);

/// Runs the consistency checks on @p backup, filling problems and warnings.
void checkBackup(const RadioLayout &layout, Backup &backup);

/// Writes @p backup under @p root/<radio id>/<time>/. Returns the folder.
QString saveBackup(const Backup &backup, const QString &root, QString *error);

/// Loads a saved backup, re-hashes its files and re-runs the checks.
bool loadBackup(const QString &folder, Backup &backup, QString *error);

/// Finds the newest verified full backup under @p root, for @p radioId, or,
/// when @p radioId is empty, for the only radio with backups there.
/// Returns its folder, or an empty string with @p error set.
QString findVerifiedBackup(const QString &root, const QString &radioId, QString *error);

/// The default backup root in the user's data folder.
QString defaultBackupRoot();
