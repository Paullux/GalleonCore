// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#include "SkySun.h"

namespace galleond {

galleon::keyboard::SkyDay skyDayFor(bool followSun, const std::optional<galleon::keyboard::GeoPlace> &place, const QDate &date, double utcOffsetHours)
{
    if (!followSun || !place || !date.isValid() || !galleon::keyboard::isValidPlace(place->latitude, place->longitude))
        return {};
    return galleon::keyboard::skyDayFrom(galleon::keyboard::sunDay(place->latitude, place->longitude, date.year(), date.month(), date.day(), utcOffsetHours));
}

} // namespace galleond
