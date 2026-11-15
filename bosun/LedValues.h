// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QList>
#include <QString>
#include <QVariantList>

#include <optional>

namespace bosun {

// The list of colours that Bosun asks galleond to show, one per LED, as QML hands it over: a flat list of groups of four whole numbers,
// LED (0 to 145), red, green, blue (0 to 255), one to 146 groups. The same on Linux and on Windows, and checked here before anything is
// sent, so a bad list never leaves this window; galleond checks it again. Nothing when it is not well formed, and `why` says why.
[[nodiscard]] std::optional<QList<int>> checkedLedValues(const QVariantList &values, QString *why);

} // namespace bosun
