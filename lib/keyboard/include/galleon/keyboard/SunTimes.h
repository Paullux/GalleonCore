// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <optional>

namespace galleon::keyboard {

// When the sun rises and sets on a day, for a place on Earth, computed here with no network: from the latitude, the longitude and the date only.
// Pure: nothing is read, sent or kept; the place and the date are arguments. A place is personal data, so nothing in this file stores or logs one.
//
// The method is the "sunrise equation" (a solar noon from the mean solar anomaly, the equation of the centre, the ecliptic longitude, the declination of
// the sun, then the hour angle at which its centre is 0.833 degrees under the horizon, which counts the radius of the sun and the refraction of the
// air). It is good to about a minute away from the polar circles, far better than a sky that changes over an hour needs. The values the tests compare
// with were computed apart, with the astral library of Python (version 3.2), for several places and the days of the equinox and the solstices.
struct SunDay
{
    enum class Kind {
        Normal,      // the sun rises and sets: `sunriseHours` and `sunsetHours` are given
        MidnightSun, // the sun stays above the horizon all day (near the pole in summer)
        PolarNight,  // the sun stays under the horizon all day (near the pole in winter)
    };

    Kind kind = Kind::Normal;
    double sunriseHours = 0.0; // local time on a 24 h clock, 0 to 24 (Normal only)
    double sunsetHours = 0.0;  // the same; it is smaller than the sunrise when the sun sets after midnight
    double dayLengthHours = 0.0; // 0 to 24; the sunset is the sunrise plus this, wrapped round the day
};

// `latitude` in degrees, -90 to 90 (north positive), `longitude` in degrees, -180 to 180 (east positive), the day as year, month (1 to 12) and day of
// the month, and `utcOffsetHours` the offset of the clock in use from UTC (the one of that day, summer time included), for example 2 for Paris in June.
// A day or a place out of range gives a normal day at 6 h and 18 h, so that nothing breaks: whoever calls checks first with isValidPlace.
[[nodiscard]] SunDay sunDay(double latitude, double longitude, int year, int month, int day, double utcOffsetHours);

// True for a finite latitude from -90 to 90 and a finite longitude from -180 to 180.
[[nodiscard]] bool isValidPlace(double latitude, double longitude);

// A place on Earth, in degrees. It is personal data: it is never logged, and a value of this type is only given to the code that needs it.
struct GeoPlace
{
    double latitude = 0.0;
    double longitude = 0.0;

    friend bool operator==(const GeoPlace &, const GeoPlace &) = default;
};

// Where the sky finds the place (decision of the owner): the one of the weather widget if there is one, else the one typed in the settings of the
// sky, else none (the fixed hours of the mockup). A place that is not valid is as good as none.
[[nodiscard]] std::optional<GeoPlace> choosePlace(const std::optional<GeoPlace> &weather, const std::optional<GeoPlace> &sky);

} // namespace galleon::keyboard
