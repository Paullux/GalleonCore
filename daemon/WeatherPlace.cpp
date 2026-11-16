// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#include "WeatherPlace.h"

#include "StrictJson.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>

namespace galleond {

std::optional<galleon::keyboard::GeoPlace> parseWeatherPlace(const QByteArray &widgetsJson)
{
    if (widgetsJson.isEmpty() || widgetsJson.size() > MaxWidgetsBytes)
        return std::nullopt;
    QJsonParseError error{};
    const QJsonDocument document = QJsonDocument::fromJson(widgetsJson, &error);
    if (error.error != QJsonParseError::NoError || !document.isObject())
        return std::nullopt;
    const QJsonValue location = document.object().value(QStringLiteral("location"));
    if (!location.isObject())
        return std::nullopt;
    const QJsonObject place = location.toObject();
    const auto latitude = boundedNumber(place.value(QStringLiteral("latitude")), -90.0, 90.0);
    const auto longitude = boundedNumber(place.value(QStringLiteral("longitude")), -180.0, 180.0);
    if (!latitude || !longitude)
        return std::nullopt;
    return galleon::keyboard::GeoPlace{*latitude, *longitude};
}

QString widgetsPath()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + QStringLiteral("/galleoncore/widgets.json");
}

std::optional<galleon::keyboard::GeoPlace> readWeatherPlace(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return std::nullopt;
    return parseWeatherPlace(file.read(MaxWidgetsBytes + 1));
}

} // namespace galleond
