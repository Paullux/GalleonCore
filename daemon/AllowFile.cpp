// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#include "AllowFile.h"

#include "AtomicFile.h"
#include "StrictJson.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>

namespace galleond {
namespace {

constexpr int Version = 1;

void explain(QString *why, const QString &reason)
{
    if (why != nullptr)
        *why = reason;
}

} // namespace

QByteArray encodeAllowFile(bool allow)
{
    QJsonObject object;
    object.insert(QStringLiteral("version"), Version);
    object.insert(QStringLiteral("allow"), allow);
    return QJsonDocument(object).toJson(QJsonDocument::Indented);
}

std::optional<bool> parseAllowFile(const QByteArray &bytes, QString *why)
{
    if (bytes.isEmpty() || bytes.size() > MaxAllowFileBytes) {
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
    if (!hasExactlyKeys(object, {QStringLiteral("version"), QStringLiteral("allow")})) {
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
    return object.value(QStringLiteral("allow")).toBool();
}

bool writeAllowFile(const QString &path, bool allow)
{
    return writeFileAtomically(path, encodeAllowFile(allow));
}

std::optional<bool> readAllowFile(const QString &path, QString *why)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        explain(why, QStringLiteral("the file cannot be opened"));
        return std::nullopt;
    }
    return parseAllowFile(file.read(MaxAllowFileBytes + 1), why);
}

bool allowFileSaysYes(const QString &path)
{
    return readAllowFile(path).value_or(false);
}

} // namespace galleond
