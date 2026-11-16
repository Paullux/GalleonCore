// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <galleon/keyboard/SkyPalette.h>
#include <galleon/keyboard/SunTimes.h>

#include <QDate>

#include <optional>

namespace galleond {

// When the sky changes on a given day, for the keyboard: the real sunrise and sunset of the place when the sky follows the sun and a place is known,
// the fixed schedule of the mockup of Aaryush1 otherwise (no choice, no place, a place or a date out of range, a sun that does not rise or does not set).
// Pure: the place, the date and the offset of the clock from UTC (summer time included) are arguments. The place is personal data: nothing here logs it.
[[nodiscard]] galleon::keyboard::SkyDay skyDayFor(bool followSun, const std::optional<galleon::keyboard::GeoPlace> &place, const QDate &date, double utcOffsetHours);

} // namespace galleond
