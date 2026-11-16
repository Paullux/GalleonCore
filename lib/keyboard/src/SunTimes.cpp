// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "galleon/keyboard/SunTimes.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <numbers>

namespace galleon::keyboard {

namespace {

constexpr double DegreesToRadians = std::numbers::pi / 180.0;
constexpr double SunriseAltitude = -0.833;       // degrees: the centre of the sun, with its radius and the refraction of the air
constexpr double J2000 = 2451545.0;              // the Julian date of 2000-01-01 12:00
constexpr double UnixEpochJulian = 2440587.5;    // the Julian date of 1970-01-01 00:00 UTC
constexpr double DaysFromUnixEpochToJ2000 = 10957.0;
constexpr double ObliquityOfTheEcliptic = 23.4397; // degrees

[[nodiscard]] double sinDegrees(double degrees)
{
    return std::sin(degrees * DegreesToRadians);
}

[[nodiscard]] double wrapDegrees(double degrees)
{
    double wrapped = std::fmod(degrees, 360.0);
    if (wrapped < 0.0)
        wrapped += 360.0;
    return wrapped;
}

[[nodiscard]] double wrapHours(double hours)
{
    double wrapped = std::fmod(hours, 24.0);
    if (wrapped < 0.0)
        wrapped += 24.0;
    return wrapped;
}

// Days from 1970-01-01 to a date of the civil calendar (proleptic Gregorian), for a month of 1 to 12.
[[nodiscard]] double daysFromUnixEpoch(int year, int month, int day)
{
    const int y = month <= 2 ? year - 1 : year;
    const int era = (y >= 0 ? y : y - 399) / 400;
    const int yearOfEra = y - era * 400;
    const int monthIndex = (month + 9) % 12; // March is 0
    const int dayOfYear = (153 * monthIndex + 2) / 5 + day - 1;
    const int dayOfEra = yearOfEra * 365 + yearOfEra / 4 - yearOfEra / 100 + dayOfYear;
    return static_cast<double>(era) * 146097.0 + dayOfEra - 719468.0;
}

[[nodiscard]] bool isValidDate(int year, int month, int day)
{
    constexpr std::array<int, 12> DaysInMonth{31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (year < 1900 || year > 2200 || month < 1 || month > 12 || day < 1)
        return false;
    return day <= DaysInMonth.at(static_cast<std::size_t>(month - 1)); // 29 February is accepted in any year: the sun does not change by a day
}

// A Julian date to hours of the day in UTC, 0 to 24.
[[nodiscard]] double utcHoursOf(double julian)
{
    return wrapHours((julian - UnixEpochJulian) * 24.0);
}

} // namespace

bool isValidPlace(double latitude, double longitude)
{
    return std::isfinite(latitude) && std::isfinite(longitude) && std::abs(latitude) <= 90.0 && std::abs(longitude) <= 180.0;
}

SunDay sunDay(double latitude, double longitude, int year, int month, int day, double utcOffsetHours)
{
    constexpr SunDay Fallback{SunDay::Kind::Normal, 6.0, 18.0, 12.0};
    if (!isValidPlace(latitude, longitude) || !isValidDate(year, month, day) || !std::isfinite(utcOffsetHours) || std::abs(utcOffsetHours) > 15.0)
        return Fallback;

    const double days = daysFromUnixEpoch(year, month, day);
    const double n = days - DaysFromUnixEpochToJ2000;      // the day number from J2000
    const double meanSolarNoon = n - longitude / 360.0;    // east longitudes bring the noon earlier
    const double anomaly = wrapDegrees(357.5291 + 0.98560028 * meanSolarNoon);
    const double centre = 1.9148 * sinDegrees(anomaly) + 0.0200 * sinDegrees(2.0 * anomaly) + 0.0003 * sinDegrees(3.0 * anomaly);
    const double eclipticLongitude = wrapDegrees(anomaly + centre + 180.0 + 102.9372);
    const double transit = J2000 + meanSolarNoon + 0.0053 * sinDegrees(anomaly) - 0.0069 * sinDegrees(2.0 * eclipticLongitude);
    const double sinDeclination = sinDegrees(eclipticLongitude) * sinDegrees(ObliquityOfTheEcliptic);
    const double cosDeclination = std::sqrt(1.0 - sinDeclination * sinDeclination);
    const double sinLatitude = sinDegrees(latitude);
    const double cosLatitude = std::cos(latitude * DegreesToRadians);

    const double denominator = cosLatitude * cosDeclination;
    const double numerator = sinDegrees(SunriseAltitude) - sinLatitude * sinDeclination;
    // At a pole the denominator is nothing: the sun is up or down all day, by the sign of the numerator.
    double cosHourAngle = numerator / denominator;
    if (std::abs(denominator) < 1e-12)
        cosHourAngle = numerator < 0.0 ? -2.0 : 2.0;
    if (cosHourAngle < -1.0)
        return {SunDay::Kind::MidnightSun, 0.0, 0.0, 24.0};
    if (cosHourAngle > 1.0)
        return {SunDay::Kind::PolarNight, 0.0, 0.0, 0.0};

    const double hourAngle = std::acos(cosHourAngle) / DegreesToRadians; // degrees: 15 degrees an hour
    const double rise = transit - hourAngle / 360.0;
    const double set = transit + hourAngle / 360.0;
    return {SunDay::Kind::Normal, wrapHours(utcHoursOf(rise) + utcOffsetHours), wrapHours(utcHoursOf(set) + utcOffsetHours), 2.0 * hourAngle / 15.0};
}

} // namespace galleon::keyboard
