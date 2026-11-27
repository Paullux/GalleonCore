// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#include "RingSettings.h"

#include "AtomicFile.h"
#include "StrictJson.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QStandardPaths>

#include <algorithm>

namespace galleond {
namespace {

constexpr int Version = 2;
constexpr int VersionWithoutColours = 1;
constexpr std::uint32_t White = 0xffffff;

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

QString ringColourText(const std::optional<std::uint32_t> &choice)
{
    if (!choice)
        return QStringLiteral("default");
    return QStringLiteral("#%1").arg(*choice & 0xffffffU, 6, 16, QLatin1Char('0'));
}

bool parseRingColour(const QString &text, std::optional<std::uint32_t> *choice)
{
    static const QRegularExpression pattern(QStringLiteral("^#[0-9a-fA-F]{6}$"));
    std::optional<std::uint32_t> parsed;
    if (text != QStringLiteral("default")) {
        if (!pattern.match(text).hasMatch())
            return false;
        bool ok = false;
        const std::uint32_t value = text.mid(1).toUInt(&ok, 16);
        if (!ok)
            return false;
        parsed = value;
    }
    if (choice != nullptr)
        *choice = parsed;
    return true;
}

QByteArray encodeRingSettings(const RingSettings &settings)
{
    QJsonObject object;
    object.insert(QStringLiteral("version"), Version);
    object.insert(QStringLiteral("lit"), settings.lit);
    object.insert(QStringLiteral("level"), settings.level);
    object.insert(QStringLiteral("left"), ringColourText(settings.left));
    object.insert(QStringLiteral("right"), ringColourText(settings.right));
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
    // The version first: it says which keys the file has. Version 1 is the file of before the colours (three keys); version 2 adds `left` and `right`.
    const auto version = wholeNumber(object.value(QStringLiteral("version")), VersionWithoutColours, Version);
    if (!version) {
        explain(why, QStringLiteral("an unknown version"));
        return std::nullopt;
    }
    const bool withColours = *version == Version;
    QStringList keys = {QStringLiteral("version"), QStringLiteral("lit"), QStringLiteral("level")};
    if (withColours)
        keys << QStringLiteral("left") << QStringLiteral("right");
    if (!hasExactlyKeys(object, keys)) {
        explain(why, QStringLiteral("unexpected or missing keys"));
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
    RingSettings settings{.lit = object.value(QStringLiteral("lit")).toBool(), .level = *level};
    if (withColours) {
        const QJsonValue left = object.value(QStringLiteral("left"));
        const QJsonValue right = object.value(QStringLiteral("right"));
        if (!left.isString() || !right.isString() || !parseRingColour(left.toString(), &settings.left)
            || !parseRingColour(right.toString(), &settings.right)) {
            explain(why, QStringLiteral("a ring colour is not \"default\" or #rrggbb"));
            return std::nullopt;
        }
    }
    return settings;
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

std::uint32_t leftRingBase(std::uint32_t themeRgb, const RingSettings &settings)
{
    return settings.left.value_or(themeRgb);
}

std::uint32_t rightRingBase(const RingSettings &settings)
{
    return settings.right.value_or(White);
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

std::uint32_t floorRingColour(std::uint32_t rgb, const RingSettings &settings)
{
    // The level of the settings, raised to the minimum; put out, the minimum. Then the same scaling as for the ambient colours.
    const int level = settings.lit ? std::max(settings.level, RingShownMinLevel) : RingShownMinLevel;
    return ambientRingColour(rgb, RingSettings{.lit = true, .level = level});
}

std::uint32_t answerRingColour(std::uint32_t rgb, const RingSettings &settings)
{
    if (!settings.lit || settings.level == 0)
        return 0; // put out, or at 0: nothing relights the rings in normal use
    return floorRingColour(rgb, settings);
}

} // namespace galleond
