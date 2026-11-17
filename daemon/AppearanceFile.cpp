// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#include "AppearanceFile.h"

#include "AtomicFile.h"
#include "StrictJson.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QStringList>

namespace galleond {
namespace {

using galleon::keyboard::Appearance;
using galleon::keyboard::BaseType;
using galleon::keyboard::LevelFull;
using galleon::keyboard::MaxRainbowSpeed;
using galleon::keyboard::Retouch;
using galleon::keyboard::Rgb;

constexpr int Version = 1;

void explain(QString *why, const QString &reason)
{
    if (why != nullptr)
        *why = reason;
}

// Three whole numbers 0 to 255, as a JSON array of exactly three.
std::optional<Rgb> colourFrom(const QJsonValue &value)
{
    if (!value.isArray())
        return std::nullopt;
    const QJsonArray numbers = value.toArray();
    if (numbers.size() != 3)
        return std::nullopt;
    const auto red = wholeNumber(numbers.at(0), 0, 255);
    const auto green = wholeNumber(numbers.at(1), 0, 255);
    const auto blue = wholeNumber(numbers.at(2), 0, 255);
    if (!red || !green || !blue)
        return std::nullopt;
    return Rgb{static_cast<std::uint8_t>(*red), static_cast<std::uint8_t>(*green), static_cast<std::uint8_t>(*blue)};
}

std::optional<Retouch> retouchFrom(const QJsonValue &value)
{
    if (!value.isArray())
        return std::nullopt;
    const QJsonArray numbers = value.toArray();
    if (numbers.size() != 4)
        return std::nullopt;
    const auto led = wholeNumber(numbers.at(0), 0, static_cast<int>(galleon::keyboard::LiveLedCount) - 1);
    const auto colour = colourFrom(QJsonArray{numbers.at(1), numbers.at(2), numbers.at(3)});
    if (!led || !colour)
        return std::nullopt;
    return Retouch{static_cast<std::uint16_t>(*led), *colour};
}

QJsonArray colourToJson(const Rgb &colour)
{
    return QJsonArray{colour.red, colour.green, colour.blue};
}

// The keys that the base of the sky may have: its type, and optionally `animate`, `follow_sun`, and a place (`latitude` and `longitude`, together, and only
// with `follow_sun`). Nothing else.
bool isSkyBase(const QJsonObject &base)
{
    const bool follows = base.contains(QStringLiteral("follow_sun"));
    const bool latitude = base.contains(QStringLiteral("latitude"));
    const bool longitude = base.contains(QStringLiteral("longitude"));
    if (latitude != longitude || (latitude && !follows))
        return false;
    int allowed = 1; // "type"
    allowed += base.contains(QStringLiteral("animate")) ? 1 : 0;
    allowed += follows ? 1 : 0;
    allowed += latitude ? 2 : 0;
    return base.size() == allowed;
}

} // namespace

QByteArray encodeAppearance(const Appearance &appearance)
{
    QJsonObject base;
    if (appearance.base == BaseType::Rainbow) {
        base.insert(QStringLiteral("type"), QStringLiteral("rainbow"));
        base.insert(QStringLiteral("speed"), appearance.rainbowSpeed);
    } else if (appearance.base == BaseType::Sky) {
        base.insert(QStringLiteral("type"), QStringLiteral("sky"));
        if (!appearance.skyAnimated)
            base.insert(QStringLiteral("animate"), false); // the default (on) is not written: a file of someone who never asked says nothing more
        if (appearance.followSun || appearance.skyPlace) {
            base.insert(QStringLiteral("follow_sun"), appearance.followSun);
            if (appearance.skyPlace) {
                base.insert(QStringLiteral("latitude"), appearance.skyPlace->latitude);
                base.insert(QStringLiteral("longitude"), appearance.skyPlace->longitude);
            }
        }
    } else {
        base.insert(QStringLiteral("type"), QStringLiteral("uniform"));
        base.insert(QStringLiteral("colour"), colourToJson(appearance.colour));
    }
    QJsonArray overrides;
    for (const Retouch &retouch : appearance.retouches)
        overrides.append(QJsonArray{retouch.led, retouch.colour.red, retouch.colour.green, retouch.colour.blue});
    QJsonObject object;
    object.insert(QStringLiteral("version"), Version);
    object.insert(QStringLiteral("base"), base);
    object.insert(QStringLiteral("overrides"), overrides);
    object.insert(QStringLiteral("level"), appearance.level);
    return QJsonDocument(object).toJson(QJsonDocument::Indented);
}

std::optional<Appearance> parseAppearance(const QByteArray &bytes, QString *why)
{
    if (bytes.isEmpty() || bytes.size() > MaxAppearanceBytes) {
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
    if (!hasExactlyKeys(object, {QStringLiteral("version"), QStringLiteral("base"), QStringLiteral("overrides"), QStringLiteral("level")})) {
        explain(why, QStringLiteral("unexpected or missing keys"));
        return std::nullopt;
    }
    if (wholeNumber(object.value(QStringLiteral("version")), Version, Version) != Version) {
        explain(why, QStringLiteral("an unknown version"));
        return std::nullopt;
    }
    Appearance appearance;
    const auto level = wholeNumber(object.value(QStringLiteral("level")), 0, LevelFull);
    if (!level) {
        explain(why, QStringLiteral("the level is not a whole number from 0 to 1000"));
        return std::nullopt;
    }
    appearance.level = *level;
    const QJsonValue baseValue = object.value(QStringLiteral("base"));
    if (!baseValue.isObject()) {
        explain(why, QStringLiteral("the base is not an object"));
        return std::nullopt;
    }
    const QJsonObject base = baseValue.toObject();
    const QString type = base.value(QStringLiteral("type")).toString();
    if (type == QLatin1String("uniform") && hasExactlyKeys(base, {QStringLiteral("type"), QStringLiteral("colour")})) {
        const auto colour = colourFrom(base.value(QStringLiteral("colour")));
        if (!colour) {
            explain(why, QStringLiteral("the colour of the base is not three whole numbers from 0 to 255"));
            return std::nullopt;
        }
        appearance.base = BaseType::Uniform;
        appearance.colour = *colour;
    } else if (type == QLatin1String("rainbow") && hasExactlyKeys(base, {QStringLiteral("type"), QStringLiteral("speed")})) {
        const auto speed = wholeNumber(base.value(QStringLiteral("speed")), 0, MaxRainbowSpeed);
        if (!speed) {
            explain(why, QStringLiteral("the speed of the rainbow is not a whole number from 0 to 1000"));
            return std::nullopt;
        }
        appearance.base = BaseType::Rainbow;
        appearance.rainbowSpeed = *speed;
    } else if (type == QLatin1String("sky") && isSkyBase(base)) {
        appearance.base = BaseType::Sky;
        if (base.contains(QStringLiteral("animate"))) {
            if (!base.value(QStringLiteral("animate")).isBool()) {
                explain(why, QStringLiteral("animate is not true or false"));
                return std::nullopt;
            }
            appearance.skyAnimated = base.value(QStringLiteral("animate")).toBool();
        }
        if (base.contains(QStringLiteral("follow_sun"))) {
            if (!base.value(QStringLiteral("follow_sun")).isBool()) {
                explain(why, QStringLiteral("follow_sun is not true or false"));
                return std::nullopt;
            }
            appearance.followSun = base.value(QStringLiteral("follow_sun")).toBool();
        }
        if (base.contains(QStringLiteral("latitude"))) {
            const auto latitude = boundedNumber(base.value(QStringLiteral("latitude")), -90.0, 90.0);
            const auto longitude = boundedNumber(base.value(QStringLiteral("longitude")), -180.0, 180.0);
            if (!latitude || !longitude) {
                explain(why, QStringLiteral("the place of the sky is not a latitude from -90 to 90 and a longitude from -180 to 180"));
                return std::nullopt;
            }
            appearance.skyPlace = galleon::keyboard::GeoPlace{*latitude, *longitude};
        }
    } else {
        explain(why, QStringLiteral("an unknown type of base, or the wrong keys for it"));
        return std::nullopt;
    }
    const QJsonValue overridesValue = object.value(QStringLiteral("overrides"));
    if (!overridesValue.isArray() || overridesValue.toArray().size() > static_cast<qsizetype>(galleon::keyboard::LiveLedCount)) {
        explain(why, QStringLiteral("the overrides are not a list of at most one per LED"));
        return std::nullopt;
    }
    for (const auto &item : overridesValue.toArray()) {
        const auto retouch = retouchFrom(item);
        if (!retouch) {
            explain(why, QStringLiteral("an override is not [led, red, green, blue] within range"));
            return std::nullopt;
        }
        appearance.retouches.push_back(*retouch);
    }
    if (!galleon::keyboard::isValid(appearance)) { // a LED twice, among the rest
        explain(why, QStringLiteral("a LED is overridden twice"));
        return std::nullopt;
    }
    return appearance;
}

QString appearancePath()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + QStringLiteral("/galleoncore/lighting-appearance.json");
}

bool writeAppearance(const QString &path, const Appearance &appearance)
{
    // The hidden rain is given by the daemon while the easter egg runs: it is never a saved appearance.
    return appearance.base != galleon::keyboard::BaseType::Matrix && galleon::keyboard::isValid(appearance)
           && writeFileAtomically(path, encodeAppearance(appearance));
}

std::optional<Appearance> readAppearance(const QString &path, QString *why)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        explain(why, QStringLiteral("the file cannot be opened"));
        return std::nullopt;
    }
    return parseAppearance(file.read(MaxAppearanceBytes + 1), why);
}

} // namespace galleond
