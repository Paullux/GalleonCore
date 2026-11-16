// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <galleon/keyboard/SunTimes.h>

#include <QByteArray>
#include <QString>

#include <optional>

namespace galleond {

// The place that the person gave to the weather widget (widgets.json, the key "location": a latitude and a longitude), read for the sky that follows the
// sun. The weather widget stores it as soon as it is typed in Bosun, even while the weather card stays off. The keyboard side of galleond reads the file
// by itself: it does not go through HomeWidgets (the module side), as AGENTS.md requires.
//
// widgets.json is the person's file and may be damaged or edited by hand, so it is untrusted input: a bounded size, a JSON object, and of all that it
// holds only the two numbers of the place, finite and in range. Anything else, or nothing, gives no place. A place is personal data: nothing here logs it.
inline constexpr qint64 MaxWidgetsBytes = 1 << 20; // 1 MiB: the file of the widgets is a few kilobytes

[[nodiscard]] std::optional<galleon::keyboard::GeoPlace> parseWeatherPlace(const QByteArray &widgetsJson);

// ~/.config/galleoncore/widgets.json, where HomeWidgets writes it.
[[nodiscard]] QString widgetsPath();
// Nothing for a missing, unreadable, oversized or damaged file, or one with no place.
[[nodiscard]] std::optional<galleon::keyboard::GeoPlace> readWeatherPlace(const QString &path);

} // namespace galleond
