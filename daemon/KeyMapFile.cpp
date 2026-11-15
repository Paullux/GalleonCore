// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#include "KeyMapFile.h"

#include "AtomicFile.h"
#include "StrictJson.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QStringList>

#include <algorithm>
#include <cmath>

namespace galleond {
namespace {

using galleon::keyboard::LedAssignment;
using galleon::keyboard::LedKind;

constexpr int Version = 1;
constexpr QLatin1String KeyboardDevice("1b1c:2b0c"); // the keyboard part (AGENTS.md, Hardware facts)

void explain(QString *why, const QString &reason)
{
    if (why != nullptr)
        *why = reason;
}

QJsonObject entryToJson(const LedAssignment &entry)
{
    QJsonObject object;
    object.insert(QStringLiteral("led"), static_cast<int>(entry.led));
    switch (entry.kind) {
    case LedKind::Key:
        object.insert(QStringLiteral("kind"), QStringLiteral("key"));
        object.insert(QStringLiteral("key"), static_cast<int>(entry.key));
        break;
    case LedKind::Strip:
        object.insert(QStringLiteral("kind"), QStringLiteral("strip"));
        object.insert(QStringLiteral("position"), static_cast<int>(entry.stripPosition));
        break;
    case LedKind::Unlit:
        object.insert(QStringLiteral("kind"), QStringLiteral("unlit"));
        break;
    }
    return object;
}

std::optional<LedAssignment> entryFromJson(const QJsonObject &object, QString *why)
{
    const auto led = wholeNumber(object.value(QStringLiteral("led")), 0, 65535);
    const QJsonValue kind = object.value(QStringLiteral("kind"));
    if (!led || !kind.isString()) {
        explain(why, QStringLiteral("an entry has no valid led or kind"));
        return std::nullopt;
    }
    LedAssignment entry;
    entry.led = static_cast<std::uint16_t>(*led);
    const QString name = kind.toString();
    if (name == QLatin1String("key")) {
        const auto key = hasExactlyKeys(object, {QStringLiteral("led"), QStringLiteral("kind"), QStringLiteral("key")})
            ? wholeNumber(object.value(QStringLiteral("key")), 0, 0xFFFF)
            : std::nullopt;
        if (!key) {
            explain(why, QStringLiteral("a key entry is malformed"));
            return std::nullopt;
        }
        entry.kind = LedKind::Key;
        entry.key = static_cast<galleon::keyboard::HidUsage>(*key);
    } else if (name == QLatin1String("strip")) {
        const auto position = hasExactlyKeys(object, {QStringLiteral("led"), QStringLiteral("kind"), QStringLiteral("position")})
            ? wholeNumber(object.value(QStringLiteral("position")), 0, 255)
            : std::nullopt;
        if (!position) {
            explain(why, QStringLiteral("a strip entry is malformed"));
            return std::nullopt;
        }
        entry.kind = LedKind::Strip;
        entry.stripPosition = static_cast<std::uint8_t>(*position);
    } else if (name == QLatin1String("unlit") && hasExactlyKeys(object, {QStringLiteral("led"), QStringLiteral("kind")})) {
        entry.kind = LedKind::Unlit;
    } else {
        explain(why, QStringLiteral("an entry has an unknown kind"));
        return std::nullopt;
    }
    return entry;
}

} // namespace

QByteArray encodeKeyMap(const KeyMapFile &file)
{
    QJsonArray leds;
    for (const LedAssignment &entry : file.map.assignments())
        leds.append(entryToJson(entry));
    QJsonObject object;
    object.insert(QStringLiteral("version"), Version);
    object.insert(QStringLiteral("device"), QString(KeyboardDevice));
    object.insert(QStringLiteral("source"), file.source.left(MaxKeyMapSource));
    if (!file.firmware.isEmpty())
        object.insert(QStringLiteral("firmware"), file.firmware.left(MaxKeyMapFirmware));
    object.insert(QStringLiteral("leds"), leds);
    return QJsonDocument(object).toJson(QJsonDocument::Indented);
}

std::optional<KeyMapFile> parseKeyMap(const QByteArray &bytes, QString *why)
{
    if (bytes.isEmpty() || bytes.size() > MaxKeyMapBytes) {
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
    const bool withFirmware = object.contains(QStringLiteral("firmware"));
    QStringList expected = {QStringLiteral("version"), QStringLiteral("device"), QStringLiteral("source"), QStringLiteral("leds")};
    if (withFirmware)
        expected.append(QStringLiteral("firmware"));
    if (!hasExactlyKeys(object, expected)) {
        explain(why, QStringLiteral("unexpected or missing keys"));
        return std::nullopt;
    }
    const QJsonValue source = object.value(QStringLiteral("source"));
    const QJsonValue firmware = object.value(QStringLiteral("firmware"));
    const QJsonValue leds = object.value(QStringLiteral("leds"));
    if (wholeNumber(object.value(QStringLiteral("version")), Version, Version) != Version
        || object.value(QStringLiteral("device")).toString() != KeyboardDevice || !source.isString() || !leds.isArray()
        || (withFirmware && !firmware.isString())) {
        explain(why, QStringLiteral("a header field has the wrong type or value"));
        return std::nullopt;
    }
    if (source.toString().isEmpty() || source.toString().size() > MaxKeyMapSource || !isPlainText(source.toString())
        || firmware.toString().size() > MaxKeyMapFirmware || !isPlainText(firmware.toString())) {
        explain(why, QStringLiteral("the source or the firmware is empty, too long or not plain text"));
        return std::nullopt;
    }
    const QJsonArray entries = leds.toArray();
    if (entries.size() > static_cast<qsizetype>(galleon::keyboard::LiveLedCount)) {
        explain(why, QStringLiteral("more entries than LEDs"));
        return std::nullopt;
    }
    KeyMapFile result;
    result.source = source.toString();
    result.firmware = firmware.toString();
    for (const auto &value : entries) {
        if (!value.isObject()) {
            explain(why, QStringLiteral("an entry is not an object"));
            return std::nullopt;
        }
        const auto entry = entryFromJson(value.toObject(), why);
        if (!entry)
            return std::nullopt;
        if (result.map.assignmentFor(entry->led).has_value()) {
            explain(why, QStringLiteral("a LED is listed twice"));
            return std::nullopt;
        }
        if (result.map.assign(*entry).has_value()) {
            explain(why, QStringLiteral("an entry is refused by the map (LED out of range, unknown key, key used twice, bad position)"));
            return std::nullopt;
        }
    }
    return result;
}

QString keyMapPath()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + QStringLiteral("/galleoncore/keymap.json");
}

bool writeKeyMap(const QString &path, const KeyMapFile &file)
{
    return writeFileAtomically(path, encodeKeyMap(file));
}

std::optional<KeyMapFile> readKeyMap(const QString &path, QString *why)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        explain(why, QStringLiteral("the file cannot be opened"));
        return std::nullopt;
    }
    return parseKeyMap(file.read(MaxKeyMapBytes + 1), why);
}

} // namespace galleond
