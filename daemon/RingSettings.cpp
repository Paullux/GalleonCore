// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#include "RingSettings.h"

#include "AtomicFile.h"
#include "StrictJson.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>

#include <algorithm>

namespace galleond {
namespace {

constexpr int Version = 1;

void explain(QString *why, const QString &reason)
{
    if (why != nullptr)
        *why = reason;
}

bool inRange(const RingSettings &settings)
{
    return settings.level >= 0 && settings.level <= RingLevelFull;
}

} // namespace

QByteArray encodeRingSettings(const RingSettings &settings)
{
    QJsonObject object;
    object.insert(QStringLiteral("version"), Version);
    object.insert(QStringLiteral("lit"), settings.lit);
    object.insert(QStringLiteral("level"), settings.level);
    return QJsonDocument(object).toJson(QJsonDocument::Indented);
}

std::optional<RingSettings> parseRingSettings(const QByteArray &bytes, QString *why)
{
    if (bytes.isEmpty() || bytes.size() > MaxRingSettingsBytes) {
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
    if (!hasExactlyKeys(object, {QStringLiteral("version"), QStringLiteral("lit"), QStringLiteral("level")})) {
        explain(why, QStringLiteral("unexpected or missing keys"));
        return std::nullopt;
    }
    if (wholeNumber(object.value(QStringLiteral("version")), Version, Version) != Version) {
        explain(why, QStringLiteral("an unknown version"));
        return std::nullopt;
    }
    if (!object.value(QStringLiteral("lit")).isBool()) {
        explain(why, QStringLiteral("lit is not true or false"));
        return std::nullopt;
    }
    const auto level = wholeNumber(object.value(QStringLiteral("level")), 0, RingLevelFull);
    if (!level) {
        explain(why, QStringLiteral("the level is not a whole number from 0 to 1000"));
        return std::nullopt;
    }
    return RingSettings{object.value(QStringLiteral("lit")).toBool(), *level};
}

QString ringSettingsPath()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + QStringLiteral("/galleoncore/dial-rings.json");
}

bool writeRingSettings(const QString &path, const RingSettings &settings)
{
    return inRange(settings) && writeFileAtomically(path, encodeRingSettings(settings));
}

std::optional<RingSettings> readRingSettings(const QString &path, QString *why)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        explain(why, QStringLiteral("the file cannot be opened"));
        return std::nullopt;
    }
    return parseRingSettings(file.read(MaxRingSettingsBytes + 1), why);
}

std::uint32_t ambientRingColour(std::uint32_t rgb, const RingSettings &settings)
{
    if (!settings.lit)
        return 0;
    const auto scaled = [&](int shift) {
        const std::uint32_t channel = (rgb >> shift) & 0xffU;
        const std::uint32_t level = static_cast<std::uint32_t>(std::clamp(settings.level, 0, RingLevelFull));
        return (channel * level + RingLevelFull / 2) / RingLevelFull; // to the nearest
    };
    return (scaled(16) << 16) | (scaled(8) << 8) | scaled(0);
}

} // namespace galleond
