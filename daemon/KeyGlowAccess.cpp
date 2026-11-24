// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#include "KeyGlowAccess.h"

#include "AtomicFile.h"
#include "StrictJson.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QStandardPaths>

namespace galleond {
namespace {

constexpr int Version = 1;

void explain(QString *why, const QString &reason)
{
    if (why != nullptr)
        *why = reason;
}

} // namespace

QString keyGlowColourText(galleon::keyboard::Rgb colour)
{
    return QStringLiteral("#%1%2%3")
        .arg(colour.red, 2, 16, QLatin1Char('0'))
        .arg(colour.green, 2, 16, QLatin1Char('0'))
        .arg(colour.blue, 2, 16, QLatin1Char('0'));
}

std::optional<galleon::keyboard::Rgb> parseKeyGlowColour(const QString &text)
{
    static const QRegularExpression pattern(QStringLiteral("^#[0-9a-fA-F]{6}$"));
    if (!pattern.match(text).hasMatch())
        return std::nullopt;
    bool ok = false;
    const unsigned value = text.mid(1).toUInt(&ok, 16);
    if (!ok)
        return std::nullopt;
    return galleon::keyboard::Rgb{static_cast<std::uint8_t>(value >> 16U & 0xFFU), static_cast<std::uint8_t>(value >> 8U & 0xFFU), static_cast<std::uint8_t>(value & 0xFFU)};
}

QByteArray encodeKeyGlowChoice(const KeyGlowChoice &choice)
{
    QJsonObject object;
    object.insert(QStringLiteral("version"), Version);
    object.insert(QStringLiteral("allow"), choice.allow);
    object.insert(QStringLiteral("colour"), keyGlowColourText(choice.colour));
    return QJsonDocument(object).toJson(QJsonDocument::Indented);
}

std::optional<KeyGlowChoice> parseKeyGlowChoice(const QByteArray &bytes, QString *why)
{
    if (bytes.isEmpty() || bytes.size() > MaxKeyGlowAccessBytes) {
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
    if (!hasExactlyKeys(object, {QStringLiteral("version"), QStringLiteral("allow"), QStringLiteral("colour")})) {
        explain(why, QStringLiteral("unexpected or missing keys"));
        return std::nullopt;
    }
    if (wholeNumber(object.value(QStringLiteral("version")), Version, Version) != Version) {
        explain(why, QStringLiteral("an unknown version"));
        return std::nullopt;
    }
    if (!object.value(QStringLiteral("allow")).isBool()) {
        explain(why, QStringLiteral("allow is not true or false"));
        return std::nullopt;
    }
    const QJsonValue colourValue = object.value(QStringLiteral("colour"));
    if (!colourValue.isString()) {
        explain(why, QStringLiteral("colour is not a text"));
        return std::nullopt;
    }
    const auto colour = parseKeyGlowColour(colourValue.toString());
    if (!colour) {
        explain(why, QStringLiteral("colour is not written as #rrggbb"));
        return std::nullopt;
    }
    return KeyGlowChoice{object.value(QStringLiteral("allow")).toBool(), *colour};
}

QString keyGlowAccessPath()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + QStringLiteral("/galleoncore/key-glow.json");
}

bool writeKeyGlowChoice(const QString &path, const KeyGlowChoice &choice)
{
    return writeFileAtomically(path, encodeKeyGlowChoice(choice));
}

std::optional<KeyGlowChoice> readKeyGlowChoice(const QString &path, QString *why)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        explain(why, QStringLiteral("the file cannot be opened"));
        return std::nullopt;
    }
    return parseKeyGlowChoice(file.read(MaxKeyGlowAccessBytes + 1), why);
}

KeyGlowChoice keyGlowChoice(const QString &path)
{
    return readKeyGlowChoice(path).value_or(KeyGlowChoice{});
}

} // namespace galleond
