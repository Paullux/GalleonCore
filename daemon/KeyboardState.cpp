// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#include "KeyboardState.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>
#include <QByteArray>

#include <cmath>

namespace galleond {
namespace {

constexpr int Version = 1;

// A JSON number that is a whole number within [low, high].
std::optional<qint64> wholeNumber(const QJsonValue &value, double low, double high)
{
    if (!value.isDouble())
        return std::nullopt;
    const double number = value.toDouble();
    if (!std::isfinite(number) || number < low || number > high || number != std::floor(number))
        return std::nullopt;
    return static_cast<qint64>(number);
}

} // namespace

QByteArray encodeKeyboardState(const KeyboardState &state)
{
    QJsonObject object;
    object.insert(QStringLiteral("version"), Version);
    object.insert(QStringLiteral("brightness"), state.brightness);
    object.insert(QStringLiteral("time"), state.time);
    return QJsonDocument(object).toJson(QJsonDocument::Compact) + '\n';
}

std::optional<KeyboardState> parseKeyboardState(const QByteArray &bytes)
{
    if (bytes.isEmpty() || bytes.size() > MaxKeyboardStateBytes)
        return std::nullopt;
    QJsonParseError error{};
    const QJsonDocument document = QJsonDocument::fromJson(bytes, &error);
    if (error.error != QJsonParseError::NoError || !document.isObject())
        return std::nullopt;
    const QJsonObject object = document.object();
    if (object.size() != 3)
        return std::nullopt;
    const auto version = wholeNumber(object.value(QStringLiteral("version")), Version, Version);
    const auto brightness = wholeNumber(object.value(QStringLiteral("brightness")), 0, 1000);
    // 2^53 is the largest whole number a JSON double holds exactly; a time past year 3000 is not a time.
    const auto time = wholeNumber(object.value(QStringLiteral("time")), 0, 32503680000.0);
    if (!version || !brightness || !time)
        return std::nullopt;
    return KeyboardState{.brightness = static_cast<int>(*brightness), .time = *time};
}

bool isFreshKeyboardState(const KeyboardState &state, qint64 notBefore)
{
    return state.time >= notBefore;
}

QString keyboardStatePath()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + QStringLiteral("/galleoncore/keyboard-state.json");
}

bool writeKeyboardState(const QString &path, const KeyboardState &state)
{
    if (!QDir().mkpath(QFileInfo(path).absolutePath()))
        return false;
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return false;
    file.write(encodeKeyboardState(state));
    return file.commit();
}

std::optional<KeyboardState> readKeyboardState(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return std::nullopt;
    // One byte more than the limit is enough to know it is too large, without reading a huge file.
    return parseKeyboardState(file.read(MaxKeyboardStateBytes + 1));
}

} // namespace galleond
