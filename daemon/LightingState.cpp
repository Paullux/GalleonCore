// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#include "LightingState.h"

#include "AtomicFile.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>

#include <algorithm>
#include <cmath>

namespace galleond {
namespace {

constexpr int Version = 1;

bool plain(const QString &text)
{
    return std::ranges::all_of(text, [](QChar c) { return c.unicode() >= 0x20 && c.unicode() != 0x7f; });
}

} // namespace

bool isKnownLightingState(const QString &state)
{
    static const QStringList known = {QStringLiteral("off"), QStringLiteral("starting"), QStringLiteral("listening"), QStringLiteral("taking"),
                                      QStringLiteral("holding"), QStringLiteral("yielded"), QStringLiteral("sleeping")};
    return known.contains(state);
}

QByteArray encodeLightingState(const LightingState &state)
{
    QJsonObject object;
    object.insert(QStringLiteral("version"), Version);
    object.insert(QStringLiteral("state"), state.state);
    object.insert(QStringLiteral("detail"), state.detail.left(MaxLightingDetail));
    object.insert(QStringLiteral("time"), state.time);
    return QJsonDocument(object).toJson(QJsonDocument::Compact) + '\n';
}

std::optional<LightingState> parseLightingState(const QByteArray &bytes)
{
    if (bytes.isEmpty() || bytes.size() > MaxLightingStateBytes)
        return std::nullopt;
    QJsonParseError error{};
    const QJsonDocument document = QJsonDocument::fromJson(bytes, &error);
    if (error.error != QJsonParseError::NoError || !document.isObject())
        return std::nullopt;
    const QJsonObject object = document.object();
    if (object.size() != 4)
        return std::nullopt;
    const QJsonValue version = object.value(QStringLiteral("version"));
    const QJsonValue state = object.value(QStringLiteral("state"));
    const QJsonValue detail = object.value(QStringLiteral("detail"));
    const QJsonValue time = object.value(QStringLiteral("time"));
    if (!version.isDouble() || version.toDouble() != Version || !state.isString() || !detail.isString() || !time.isDouble())
        return std::nullopt;
    const double seconds = time.toDouble();
    // A time past year 3000 is not a time; a whole number keeps it exact in a JSON double.
    if (!std::isfinite(seconds) || seconds < 0 || seconds > 32503680000.0 || seconds != std::floor(seconds))
        return std::nullopt;
    if (!isKnownLightingState(state.toString()) || detail.toString().size() > MaxLightingDetail || !plain(detail.toString()))
        return std::nullopt;
    return LightingState{.state = state.toString(), .detail = detail.toString(), .time = static_cast<qint64>(seconds)};
}

bool isFreshLightingState(const LightingState &state, qint64 notBefore)
{
    return state.time >= notBefore;
}

QString lightingStatePath()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + QStringLiteral("/galleoncore/lighting-state.json");
}

bool writeLightingState(const QString &path, const LightingState &state)
{
    return writeFileAtomically(path, encodeLightingState(state));
}

std::optional<LightingState> readLightingState(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return std::nullopt;
    return parseLightingState(file.read(MaxLightingStateBytes + 1));
}

} // namespace galleond
