// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include <QStringList>

#include <algorithm>
#include <cmath>
#include <optional>

namespace galleond {

// The checks that the files read from the disk have in common (keymap.json, the appearance of the lighting): a JSON number that is a whole number
// within [low, high], and an object that has exactly the keys it should. Nothing else is accepted (not "7", not 7.5, not 1e99, not an extra key).
[[nodiscard]] inline std::optional<int> wholeNumber(const QJsonValue &value, int low, int high)
{
    if (!value.isDouble())
        return std::nullopt;
    const double number = value.toDouble();
    if (!std::isfinite(number) || number != std::floor(number) || number < low || number > high)
        return std::nullopt;
    return static_cast<int>(number);
}

// A JSON number (not a string) that is finite and within [low, high], whole or not.
[[nodiscard]] inline std::optional<double> boundedNumber(const QJsonValue &value, double low, double high)
{
    if (!value.isDouble())
        return std::nullopt;
    const double number = value.toDouble();
    if (!std::isfinite(number) || number < low || number > high)
        return std::nullopt;
    return number;
}

[[nodiscard]] inline bool hasExactlyKeys(const QJsonObject &object, const QStringList &keys)
{
    return object.size() == keys.size() && std::ranges::all_of(keys, [&object](const QString &key) { return object.contains(key); });
}

[[nodiscard]] inline bool isPlainText(const QString &text)
{
    return std::ranges::all_of(text, [](QChar c) { return c.unicode() >= 0x20 && c.unicode() != 0x7f; });
}

} // namespace galleond
