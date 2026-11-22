// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#include "LightingRestore.h"

#include "AtomicFile.h"
#include "StrictJson.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>

namespace galleond {
namespace {

constexpr int Version = 1;

void explain(QString *why, const QString &reason)
{
    if (why != nullptr)
        *why = reason;
}

// The object of a file: nothing when it is too large, not JSON, not an object, of another version or without exactly `valueKey` besides the version.
std::optional<QJsonObject> readObject(const QByteArray &bytes, const QString &valueKey, QString *why)
{
    if (bytes.isEmpty() || bytes.size() > MaxLightingRestoreBytes) {
        explain(why, QStringLiteral("the file is empty or too large"));
        return std::nullopt;
    }
    QJsonParseError error{};
    const QJsonDocument document = QJsonDocument::fromJson(bytes, &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) {
        explain(why, QStringLiteral("not a JSON object"));
        return std::nullopt;
    }
    QJsonObject object = document.object();
    if (!hasExactlyKeys(object, {QStringLiteral("version"), valueKey})) {
        explain(why, QStringLiteral("unexpected or missing keys"));
        return std::nullopt;
    }
    if (wholeNumber(object.value(QStringLiteral("version")), Version, Version) != Version) {
        explain(why, QStringLiteral("an unknown version"));
        return std::nullopt;
    }
    return object;
}

QString configFile(const QString &name)
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + QStringLiteral("/galleoncore/") + name;
}

QByteArray readBounded(const QString &path, QString *why, bool *opened)
{
    QFile file(path);
    *opened = file.open(QIODevice::ReadOnly);
    if (!*opened) {
        explain(why, QStringLiteral("the file cannot be opened"));
        return {};
    }
    return file.read(MaxLightingRestoreBytes + 1);
}

QString memoryName(LightingMemory memory)
{
    switch (memory) {
    case LightingMemory::Running:
        return QStringLiteral("running");
    case LightingMemory::CleanExit:
        return QStringLiteral("clean");
    case LightingMemory::Off:
        break;
    }
    return QStringLiteral("off");
}

} // namespace

QByteArray encodeLightingRestore(bool restore)
{
    QJsonObject object;
    object.insert(QStringLiteral("version"), Version);
    object.insert(QStringLiteral("restore"), restore);
    return QJsonDocument(object).toJson(QJsonDocument::Indented);
}

std::optional<bool> parseLightingRestore(const QByteArray &bytes, QString *why)
{
    const auto object = readObject(bytes, QStringLiteral("restore"), why);
    if (!object)
        return std::nullopt;
    if (!object->value(QStringLiteral("restore")).isBool()) {
        explain(why, QStringLiteral("restore is not true or false"));
        return std::nullopt;
    }
    return object->value(QStringLiteral("restore")).toBool();
}

QByteArray encodeLightingMemory(LightingMemory memory)
{
    QJsonObject object;
    object.insert(QStringLiteral("version"), Version);
    object.insert(QStringLiteral("memory"), memoryName(memory));
    return QJsonDocument(object).toJson(QJsonDocument::Indented);
}

std::optional<LightingMemory> parseLightingMemory(const QByteArray &bytes, QString *why)
{
    const auto object = readObject(bytes, QStringLiteral("memory"), why);
    if (!object)
        return std::nullopt;
    const QJsonValue value = object->value(QStringLiteral("memory"));
    if (value.isString()) {
        const QString name = value.toString();
        for (const LightingMemory known : {LightingMemory::Off, LightingMemory::Running, LightingMemory::CleanExit}) {
            if (name == memoryName(known))
                return known;
        }
    }
    explain(why, QStringLiteral("the memory is not off, running or clean"));
    return std::nullopt;
}

QString lightingRestorePath()
{
    return configFile(QStringLiteral("lighting-restore.json"));
}

QString lightingMemoryPath()
{
    return configFile(QStringLiteral("lighting-memory.json"));
}

bool writeLightingRestore(const QString &path, bool restore)
{
    return writeFileAtomically(path, encodeLightingRestore(restore));
}

bool writeLightingMemory(const QString &path, LightingMemory memory)
{
    return writeFileAtomically(path, encodeLightingMemory(memory));
}

std::optional<bool> readLightingRestore(const QString &path, QString *why)
{
    bool opened = false;
    const QByteArray bytes = readBounded(path, why, &opened);
    return opened ? parseLightingRestore(bytes, why) : std::nullopt;
}

std::optional<LightingMemory> readLightingMemory(const QString &path, QString *why)
{
    bool opened = false;
    const QByteArray bytes = readBounded(path, why, &opened);
    return opened ? parseLightingMemory(bytes, why) : std::nullopt;
}

bool shouldRestoreLighting(std::optional<bool> asked, std::optional<LightingMemory> memory)
{
    return asked.value_or(false) && memory.value_or(LightingMemory::Off) == LightingMemory::CleanExit;
}

bool consumeLightingRestore(const QString &restorePath, const QString &memoryPath)
{
    const std::optional<bool> asked = readLightingRestore(restorePath);
    const std::optional<LightingMemory> memory = readLightingMemory(memoryPath);
    // Back to "off" before anything else: a start that goes wrong is not repeated at the next one. If that cannot be written, the next start
    // could not tell a crash from a clean exit, so nothing is put back.
    if (!writeLightingMemory(memoryPath, LightingMemory::Off))
        return false;
    return shouldRestoreLighting(asked, memory);
}

} // namespace galleond
