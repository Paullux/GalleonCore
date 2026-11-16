// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: LGPL-3.0-or-later

// The sunrise and the sunset of a place and a day, computed with no network. The expected times were computed apart, with the astral library of
// Python (3.2), for several places and the days of the equinox and the solstices of 2026 (the clock of the place, summer time included). The
// computation is good to about a minute at these places; the high latitudes are the least exact, hence the margin of 2.7 minutes.

#include <galleon/keyboard/SunTimes.h>

#include <QTest>

#include <array>
#include <cmath>

using namespace galleon::keyboard;

namespace {

struct Reference
{
    const char *place;
    double latitude;
    double longitude;
    int year;
    int month;
    int day;
    double utcOffset;
    double sunrise;
    double sunset;
};

constexpr double Margin = 0.045; // hours, 2.7 minutes

// The distance between two times of the day, going round midnight.
double apart(double a, double b)
{
    const double d = std::abs(a - b);
    return std::min(d, 24.0 - d);
}

constexpr std::array<Reference, 15> References{{
    {"Paris", 48.8566, 2.3522, 2026, 3, 20, 1.0, 6.899, 19.050},
    {"Paris", 48.8566, 2.3522, 2026, 6, 21, 2.0, 5.788, 21.959},
    {"Paris", 48.8566, 2.3522, 2026, 12, 21, 1.0, 8.693, 16.929},
    {"Quito", -0.18, -78.47, 2026, 3, 20, -5.0, 6.303, 18.405},
    {"Quito", -0.18, -78.47, 2026, 12, 21, -5.0, 6.136, 18.266},
    {"Sydney", -33.8688, 151.2093, 2026, 6, 21, 10.0, 7.003, 16.893},
    {"Sydney", -33.8688, 151.2093, 2026, 12, 21, 11.0, 5.681, 20.086},
    {"New York", 40.7128, -74.006, 2026, 6, 21, -4.0, 5.421, 20.508},
    {"New York", 40.7128, -74.006, 2026, 12, 21, -5.0, 7.281, 16.526},
    {"Reykjavik", 64.1466, -21.9426, 2026, 6, 21, 0.0, 2.939, 0.044}, // the sun sets after midnight
    {"Reykjavik", 64.1466, -21.9426, 2026, 12, 21, 0.0, 11.386, 15.476},
    {"Tokyo", 35.6762, 139.6503, 2026, 6, 21, 9.0, 4.434, 19.003},
    {"Tokyo", 35.6762, 139.6503, 2026, 12, 21, 9.0, 6.787, 16.521},
    {"Tromso", 69.6496, 18.956, 2026, 3, 20, 1.0, 5.739, 18.017},
    {"Sydney", -33.8688, 151.2093, 2026, 3, 20, 11.0, 6.969, 19.112},
}};

} // namespace

class SunTimesTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void theSunriseAndTheSunsetAreWhereTheReferenceLibraryPutsThemWithinAFewMinutes()
    {
        for (const Reference &r : References) {
            const SunDay day = sunDay(r.latitude, r.longitude, r.year, r.month, r.day, r.utcOffset);
            QVERIFY2(day.kind == SunDay::Kind::Normal, r.place);
            QVERIFY2(apart(day.sunriseHours, r.sunrise) < Margin, qPrintable(QStringLiteral("sunrise at %1 on %2-%3").arg(r.place).arg(r.month).arg(r.day)));
            QVERIFY2(apart(day.sunsetHours, r.sunset) < Margin, qPrintable(QStringLiteral("sunset at %1 on %2-%3").arg(r.place).arg(r.month).arg(r.day)));
        }
    }

    void theDayLengthIsTheTimeBetweenTheSunriseAndTheSunsetEvenAcrossMidnight()
    {
        for (const Reference &r : References) {
            const SunDay day = sunDay(r.latitude, r.longitude, r.year, r.month, r.day, r.utcOffset);
            const double between = std::fmod(day.sunsetHours - day.sunriseHours + 24.0, 24.0);
            QVERIFY2(std::abs(between - day.dayLengthHours) < 1e-6, r.place);
        }
        // Reykjavik at the solstice: the sun sets at 0 h 02, after the sunrise of 2 h 56 of the same date was passed: the day is about 21 h 08 long.
        const SunDay reykjavik = sunDay(64.1466, -21.9426, 2026, 6, 21, 0.0);
        QVERIFY(reykjavik.sunsetHours < reykjavik.sunriseHours);
        QVERIFY(std::abs(reykjavik.dayLengthHours - 21.1) < 0.1);
    }

    void atTheEquatorTheDayIsTwelveHoursAllYearAndThePolesHaveOneSunriseAYear()
    {
        for (const int month : {3, 6, 9, 12}) {
            const SunDay day = sunDay(0.0, 0.0, 2026, month, 21, 0.0);
            QCOMPARE(day.kind, SunDay::Kind::Normal);
            QVERIFY(std::abs(day.dayLengthHours - 12.1) < 0.1);
        }
    }

    void thereIsNoSunriseUnderTheArcticCircleInWinterNorInSummerTheSunDoesNotSet()
    {
        QCOMPARE(sunDay(69.6496, 18.956, 2026, 12, 21, 1.0).kind, SunDay::Kind::PolarNight); // Tromso, polar night
        QCOMPARE(sunDay(69.6496, 18.956, 2026, 6, 21, 2.0).kind, SunDay::Kind::MidnightSun);  // Tromso, midnight sun
        QCOMPARE(sunDay(-77.85, 166.67, 2026, 12, 21, 13.0).kind, SunDay::Kind::MidnightSun); // Antarctica, the southern summer
        QCOMPARE(sunDay(-77.85, 166.67, 2026, 6, 21, 12.0).kind, SunDay::Kind::PolarNight);
        QCOMPARE(sunDay(90.0, 0.0, 2026, 6, 21, 0.0).kind, SunDay::Kind::MidnightSun); // the north pole itself, where the denominator is nothing
        QCOMPARE(sunDay(90.0, 0.0, 2026, 12, 21, 0.0).kind, SunDay::Kind::PolarNight);
        QCOMPARE(sunDay(69.6496, 18.956, 2026, 12, 21, 1.0).dayLengthHours, 0.0);
        QCOMPARE(sunDay(69.6496, 18.956, 2026, 6, 21, 2.0).dayLengthHours, 24.0);
    }

    void aPlaceOrADateOutOfRangeGivesAPlainDayAndIsNotAValidPlace()
    {
        QVERIFY(isValidPlace(48.8566, 2.3522));
        QVERIFY(isValidPlace(-90.0, 180.0));
        QVERIFY(!isValidPlace(90.1, 0.0));
        QVERIFY(!isValidPlace(0.0, -180.1));
        QVERIFY(!isValidPlace(std::nan(""), 0.0));
        QVERIFY(!isValidPlace(0.0, HUGE_VAL));
        struct Bad
        {
            double lat;
            double lon;
            int y;
            int m;
            int d;
            double off;
        };
        const std::array<Bad, 7> bad{{{91.0, 0.0, 2026, 6, 21, 0.0}, {0.0, 181.0, 2026, 6, 21, 0.0}, {0.0, 0.0, 2026, 13, 21, 0.0}, {0.0, 0.0, 2026, 2, 30, 0.0},
                                      {0.0, 0.0, 2026, 6, 0, 0.0}, {0.0, 0.0, 1800, 6, 21, 0.0}, {0.0, 0.0, 2026, 6, 21, 20.0}}};
        for (const auto &b : bad) {
            const SunDay day = sunDay(b.lat, b.lon, b.y, b.m, b.d, b.off);
            QCOMPARE(day.kind, SunDay::Kind::Normal);
            QCOMPARE(day.sunriseHours, 6.0);
            QCOMPARE(day.sunsetHours, 18.0);
        }
    }

    void theSunriseIsEarlierAtTheSolsticeOfSummerThanOfWinterAndTheOtherWayInTheSouth()
    {
        QVERIFY(sunDay(48.8566, 2.3522, 2026, 6, 21, 2.0).sunriseHours < sunDay(48.8566, 2.3522, 2026, 12, 21, 1.0).sunriseHours);
        QVERIFY(sunDay(-33.8688, 151.2093, 2026, 12, 21, 11.0).sunriseHours < sunDay(-33.8688, 151.2093, 2026, 6, 21, 10.0).sunriseHours);
    }

    void theWeatherPlaceComesFirstThenTheOneOfTheSkyThenNothing()
    {
        const GeoPlace weather{48.8566, 2.3522};
        const GeoPlace sky{35.6762, 139.6503};
        QCOMPARE(choosePlace(weather, sky).value_or(GeoPlace{}), weather);   // the weather widget wins
        QCOMPARE(choosePlace(std::nullopt, sky).value_or(GeoPlace{}), sky);  // the weather was never used: the place of the sky
        QCOMPARE(choosePlace(weather, std::nullopt).value_or(GeoPlace{}), weather);
        QVERIFY(!choosePlace(std::nullopt, std::nullopt).has_value());       // no place: the fixed hours of the mockup
        // A place that is not valid is as good as none: the next one is used.
        QCOMPARE(choosePlace(GeoPlace{95.0, 0.0}, sky).value_or(GeoPlace{}), sky);
        QVERIFY(!choosePlace(GeoPlace{95.0, 0.0}, GeoPlace{0.0, 200.0}).has_value());
    }

    void theClockOffsetOnlyMovesBothTimesAlong()
    {
        const SunDay at0 = sunDay(48.8566, 2.3522, 2026, 6, 21, 0.0);
        const SunDay at2 = sunDay(48.8566, 2.3522, 2026, 6, 21, 2.0);
        QVERIFY(apart(at0.sunriseHours + 2.0, at2.sunriseHours) < 1e-9);
        QVERIFY(apart(at0.sunsetHours + 2.0, at2.sunsetHours) < 1e-9);
        QCOMPARE(at0.dayLengthHours, at2.dayLengthHours);
    }
};

QTEST_GUILESS_MAIN(SunTimesTest)
#include "test_suntimes.moc"
