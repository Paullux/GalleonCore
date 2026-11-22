// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#include "NightMode.h"

#include "AtomicFile.h"
#include "StrictJson.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QTime>

namespace galleond {
namespace {

constexpr int Version = 1;

void explain(QString *why, const QString &reason)
{
    if (why != nullptr)
        *why = reason;
}

} // namespace

QDateTime nightEnd(const QDateTime &now, int hour, int minute)
{
    if (hour < 0 || hour > 23 || minute < 0 || minute > 59 || !now.isValid())
        return {};
    QDateTime end = now; // setTime keeps the time zone of `now`
    end.setTime(QTime(hour, minute));
    if (end <= now)
        end = end.addDays(1);
    return end;
}

QByteArray encodeNightUntil(qint64 until)
{
    QJsonObject object;
    object.insert(QStringLiteral("version"), Version);
    object.insert(QStringLiteral("until"), until);
    return QJsonDocument(object).toJson(QJsonDocument::Indented);
}

std::optional<qint64> parseNightUntil(const QByteArray &bytes, qint64 now, QString *why)
{
    if (bytes.isEmpty() || bytes.size() > MaxNightFileBytes) {
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
    if (!hasExactlyKeys(object, {QStringLiteral("version"), QStringLiteral("until")})) {
        explain(why, QStringLiteral("unexpected or missing keys"));
        return std::nullopt;
    }
    if (wholeNumber(object.value(QStringLiteral("version")), Version, Version) != Version) {
        explain(why, QStringLiteral("an unknown version"));
        return std::nullopt;
    }
    const QJsonValue value = object.value(QStringLiteral("until"));
    if (!value.isDouble()) {
        explain(why, QStringLiteral("until is not a number"));
        return std::nullopt;
    }
    const double number = value.toDouble();
    // Compared as numbers first, so that a huge value is refused before any conversion.
    if (!(number > static_cast<double>(now)) || number > static_cast<double>(now + MaxNightSeconds) || number != static_cast<double>(static_cast<qint64>(number))) {
        explain(why, QStringLiteral("the end is past, too far away, or not a whole number of seconds"));
        return std::nullopt;
    }
    return static_cast<qint64>(number);
}

QString nightModePath()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + QStringLiteral("/galleoncore/night-mode.json");
}

bool writeNightUntil(const QString &path, qint64 until)
{
    return writeFileAtomically(path, encodeNightUntil(until));
}

std::optional<qint64> readNightUntil(const QString &path, qint64 now, QString *why)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        explain(why, QStringLiteral("the file cannot be opened"));
        return std::nullopt;
    }
    return parseNightUntil(file.read(MaxNightFileBytes + 1), now, why);
}

bool removeNightFile(const QString &path)
{
    return !QFile::exists(path) || QFile::remove(path);
}

} // namespace galleond
