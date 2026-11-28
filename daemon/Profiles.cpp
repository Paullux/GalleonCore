// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Profiles.h"

#include "AtomicFile.h"
#include "StrictJson.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>

#include <algorithm>

namespace galleond {
namespace {

constexpr int Version = 1;

void explain(QString *why, const QString &reason)
{
    if (why != nullptr)
        *why = reason;
}

} // namespace

QString profilePath(const QString &configFolder, const QString &name)
{
    if (!isValidProfileName(name) || configFolder.isEmpty())
        return {};
    if (name == defaultProfileName())
        return QDir(configFolder).filePath(QStringLiteral("pages.json"));
    return QDir(configFolder).filePath(QStringLiteral("profiles/%1.json").arg(name));
}

QStringList profileNamesFrom(const QStringList &fileBaseNames)
{
    QStringList others;
    for (const QString &name : fileBaseNames) {
        if (name != defaultProfileName() && isValidProfileName(name) && !others.contains(name))
            others.append(name);
    }
    others.sort();
    QStringList names{defaultProfileName()};
    names.append(others.first(std::min<qsizetype>(others.size(), MaxProfiles - 1)));
    return names;
}

QStringList listProfiles(const QString &configFolder)
{
    QStringList bases;
    const QDir folder(QDir(configFolder).filePath(QStringLiteral("profiles")));
    for (const QFileInfo &info : folder.entryInfoList({QStringLiteral("*.json")}, QDir::Files | QDir::NoDotAndDotDot, QDir::Name))
        bases.append(info.completeBaseName());
    return profileNamesFrom(bases);
}

QString steppedProfile(const QStringList &names, const QString &active, int steps)
{
    if (names.isEmpty())
        return defaultProfileName();
    const qsizetype count = names.size();
    const qsizetype found = names.indexOf(active);
    const qsizetype from = found < 0 ? 0 : found;
    // A long step is brought back into the range first, so that nothing overflows.
    const auto shift = static_cast<qsizetype>(steps % static_cast<int>(count));
    return names.at(((from + shift) % count + count) % count);
}

QString activeProfilePath(const QString &configFolder)
{
    return QDir(configFolder).filePath(QStringLiteral("profile-active.json"));
}

QByteArray encodeActiveProfile(const QString &name)
{
    QJsonObject object;
    object.insert(QStringLiteral("version"), Version);
    object.insert(QStringLiteral("profile"), name);
    return QJsonDocument(object).toJson(QJsonDocument::Indented);
}

std::optional<QString> parseActiveProfile(const QByteArray &bytes, QString *why)
{
    if (bytes.isEmpty() || bytes.size() > MaxActiveProfileFileBytes) {
        explain(why, QStringLiteral("the file is empty or too large"));
        return std::nullopt;
    }
    QJsonParseError error{};
    const QJsonDocument document = QJsonDocument::fromJson(bytes, &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) {
        explain(why, QStringLiteral("not a JSON object"));
        return std::nullopt;
    }
    const QJsonObject object = document.object();
    if (!hasExactlyKeys(object, {QStringLiteral("version"), QStringLiteral("profile")})) {
        explain(why, QStringLiteral("unexpected or missing keys"));
        return std::nullopt;
    }
    if (wholeNumber(object.value(QStringLiteral("version")), Version, Version) != Version) {
        explain(why, QStringLiteral("an unknown version"));
        return std::nullopt;
    }
    const QJsonValue profile = object.value(QStringLiteral("profile"));
    if (!profile.isString() || !isValidProfileName(profile.toString())) {
        explain(why, QStringLiteral("the profile is not a valid name"));
        return std::nullopt;
    }
    return profile.toString();
}

bool writeActiveProfile(const QString &configFolder, const QString &name)
{
    if (!isValidProfileName(name))
        return false;
    return writeFileAtomically(activeProfilePath(configFolder), encodeActiveProfile(name));
}

QString activeProfileAtStart(const QString &configFolder)
{
    QFile file(activeProfilePath(configFolder));
    if (!file.open(QIODevice::ReadOnly))
        return defaultProfileName();
    const std::optional<QString> name = parseActiveProfile(file.read(MaxActiveProfileFileBytes + 1));
    if (!name)
        return defaultProfileName();
    // The default profile is the base file: it does not have to exist to be chosen (the built-in pages are then used, as before).
    if (*name == defaultProfileName() || QFileInfo::exists(profilePath(configFolder, *name)))
        return *name;
    return defaultProfileName();
}

} // namespace galleond
