// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#include "LedPositions.h"

#include "StrictJson.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <array>
#include <cmath>

namespace galleond {
namespace {

using galleon::keyboard::LedPosition;

constexpr double Farthest = 1000.0; // mm: a position further than a metre from the origin is not a place on a keyboard

void explain(QString *why, const QString &reason)
{
    if (why != nullptr)
        *why = reason;
}

std::optional<double> place(const QJsonValue &value)
{
    if (!value.isDouble())
        return std::nullopt;
    const double number = value.toDouble();
    if (!std::isfinite(number) || std::fabs(number) > Farthest)
        return std::nullopt;
    return number;
}

} // namespace

std::optional<std::vector<LedPosition>> parseLedPositions(const QByteArray &bytes, QString *why)
{
    if (bytes.isEmpty() || bytes.size() > MaxLedPositionsBytes) {
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
    if (wholeNumber(object.value(QStringLiteral("version")), 1, 1) != 1 || object.value(QStringLiteral("unit")).toString() != QLatin1String("mm")
        || !object.value(QStringLiteral("leds")).isArray()) {
        explain(why, QStringLiteral("not a version 1 file in millimetres with a list of LEDs"));
        return std::nullopt;
    }
    std::vector<LedPosition> positions;
    std::array<bool, galleon::keyboard::LiveLedCount> seen{};
    for (const auto &item : object.value(QStringLiteral("leds")).toArray()) {
        const QJsonObject entry = item.toObject();
        const auto led = wholeNumber(entry.value(QStringLiteral("led")), 0, static_cast<int>(galleon::keyboard::LiveLedCount) - 1);
        const QString kind = entry.value(QStringLiteral("kind")).toString();
        const auto x = place(entry.value(QStringLiteral("x")));
        const auto y = place(entry.value(QStringLiteral("y")));
        if (!item.isObject() || !led || !x || !y || (kind != QLatin1String("key") && kind != QLatin1String("strip"))) {
            explain(why, QStringLiteral("an entry has no valid LED, kind or place"));
            return std::nullopt;
        }
        if (seen.at(static_cast<std::size_t>(*led))) {
            explain(why, QStringLiteral("a LED is listed twice"));
            return std::nullopt;
        }
        seen.at(static_cast<std::size_t>(*led)) = true;
        positions.push_back({static_cast<std::uint16_t>(*led), *x, *y});
    }
    if (positions.empty()) {
        explain(why, QStringLiteral("no LED"));
        return std::nullopt;
    }
    return positions;
}

const std::vector<LedPosition> &builtInLedPositions()
{
    static const std::vector<LedPosition> positions = [] {
        QFile file(QStringLiteral(":/galleond/galleon-100-sd-led-positions.json"));
        if (!file.open(QIODevice::ReadOnly))
            return std::vector<LedPosition>{};
        return parseLedPositions(file.read(MaxLedPositionsBytes + 1)).value_or(std::vector<LedPosition>{});
    }();
    return positions;
}

} // namespace galleond
