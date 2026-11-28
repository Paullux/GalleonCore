// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ProfileStore.h"

#include "AtomicFile.h"
#include "PagesConfig.h"
#include "Profiles.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

namespace galleond {
namespace {

ProfileResult refused(const QString &why)
{
    return {false, why};
}

bool isOtherThanDefault(const QString &name)
{
    return isValidProfileName(name) && name != defaultProfileName();
}

} // namespace

QString deletedProfileFileName(const QString &name, const QString &stamp, int attempt)
{
    return attempt <= 1 ? QStringLiteral("%1-%2.json").arg(name, stamp) : QStringLiteral("%1-%2-%3.json").arg(name, stamp).arg(attempt);
}

ProfileResult createProfile(const QString &configFolder, const QString &name, const QString &from, const QByteArray &fallback)
{
    if (!isOtherThanDefault(name))
        return refused(QStringLiteral("The name of a profile is 1 to %1 characters among a to z, 0 to 9 and -, and not \"default\".").arg(MaxProfileNameLength));
    if (!isValidProfileName(from))
        return refused(QStringLiteral("The profile to copy does not exist."));
    const QString target = profilePath(configFolder, name);
    if (QFileInfo::exists(target))
        return refused(QStringLiteral("A profile with this name already exists."));
    if (listProfiles(configFolder).size() >= MaxProfiles)
        return refused(QStringLiteral("There are already %1 profiles.").arg(MaxProfiles));

    QByteArray content;
    const QString source = profilePath(configFolder, from);
    QFile file(source);
    if (file.exists()) {
        if (!file.open(QIODevice::ReadOnly))
            return refused(QStringLiteral("The profile to copy cannot be read."));
        content = file.read(MaxPagesFileBytes + 1);
        if (content.size() > MaxPagesFileBytes)
            return refused(QStringLiteral("The profile to copy is too large."));
    } else if (from == defaultProfileName() && !fallback.isEmpty()) {
        content = fallback; // no pages.json yet: the built-in pages are what is shown, and what is copied
    } else {
        return refused(QStringLiteral("The profile to copy does not exist."));
    }
    QString why;
    if (!writeFileAtomically(target, content, QFileDevice::ReadOwner | QFileDevice::WriteOwner, &why))
        return refused(QStringLiteral("The profile could not be written: %1").arg(why));
    return {true, {}};
}

ProfileResult renameProfile(const QString &configFolder, const QString &from, const QString &to)
{
    if (!isOtherThanDefault(from))
        return refused(QStringLiteral("The base profile \"default\" cannot be renamed."));
    if (!isOtherThanDefault(to))
        return refused(QStringLiteral("The name of a profile is 1 to %1 characters among a to z, 0 to 9 and -, and not \"default\".").arg(MaxProfileNameLength));
    const QString source = profilePath(configFolder, from);
    const QString target = profilePath(configFolder, to);
    if (!QFileInfo::exists(source))
        return refused(QStringLiteral("The profile does not exist."));
    if (from == to || QFileInfo::exists(target))
        return refused(QStringLiteral("A profile with this name already exists."));
    if (!QFile::rename(source, target))
        return refused(QStringLiteral("The profile could not be renamed."));
    return {true, {}};
}

ProfileResult deleteProfile(const QString &configFolder, const QString &name, const QString &stamp)
{
    if (!isOtherThanDefault(name))
        return refused(QStringLiteral("The base profile \"default\" cannot be deleted."));
    const QString source = profilePath(configFolder, name);
    if (!QFileInfo::exists(source))
        return refused(QStringLiteral("The profile does not exist."));
    const QDir trash(QDir(configFolder).filePath(QStringLiteral("profiles/.deleted")));
    if (!QDir().mkpath(trash.path()))
        return refused(QStringLiteral("The folder of deleted profiles cannot be made."));
    // A file that is already there (the same name deleted twice in a second) is never replaced: the next free name is taken.
    for (int attempt = 1; attempt <= 99; ++attempt) {
        const QString target = trash.filePath(deletedProfileFileName(name, stamp, attempt));
        if (QFileInfo::exists(target))
            continue;
        if (!QFile::rename(source, target))
            return refused(QStringLiteral("The profile could not be moved to the deleted ones."));
        return {true, {}};
    }
    return refused(QStringLiteral("Too many deleted profiles of this name."));
}

} // namespace galleond
