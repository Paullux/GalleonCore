// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ProfileNames.h"

#include <algorithm>

namespace galleond {

QString defaultProfileName()
{
    return QStringLiteral("default");
}

bool isValidProfileName(const QString &name)
{
    if (name == defaultProfileName())
        return true;
    // Spelled out, not a pattern with $: "$" matches before a final newline, and a name with one must not pass.
    if (name.isEmpty() || name.size() > MaxProfileNameLength || name.front() == QLatin1Char('-'))
        return false;
    return std::ranges::all_of(name, [](QChar c) {
        return (c >= QLatin1Char('a') && c <= QLatin1Char('z')) || (c >= QLatin1Char('0') && c <= QLatin1Char('9')) || c == QLatin1Char('-');
    });
}

} // namespace galleond
