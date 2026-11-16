// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: LGPL-3.0-or-later

// The sky that follows the hour: the palette of the mockup of Aaryush1 blended between its keyframes. The expected colours were computed
// apart, in Python, with the formula of the mockup (each channel `round(from + (to - from) * fraction)` between two keyframes).

#include <galleon/keyboard/SkyPalette.h>

#include <QTest>

#include <array>
#include <cmath>
#include <utility>

using namespace galleon::keyboard;

namespace {

struct Hour
{
    double hours = 0.0;
    Rgb top;
    Rgb bottom;
};

} // namespace

class SkyPaletteTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void theNightHoldsUntilFiveAndAfterTheEndOfDusk()
    {
        for (const double hours : {0.0, 3.0, 5.0, 19.25, 23.5, 24.0}) {
            const auto rows = skyRowColours(hours);
            QCOMPARE(rows.front(), (Rgb{10, 14, 38}));
            QCOMPARE(rows.back(), (Rgb{16, 22, 56}));
        }
    }

    void theColoursBetweenTwoKeyframesAreTheLinearBlendOfEachChannel()
    {
        const std::array<Hour, 9> known{{
            {5.625, {22, 22, 65}, {136, 101, 73}},   // halfway from night to dawn
            {6.25, {34, 30, 92}, {255, 180, 90}},    // the dawn itself
            {6.875, {30, 50, 131}, {158, 158, 160}}, // halfway from dawn to day
            {7.5, {26, 70, 170}, {60, 135, 230}},    // the day
            {12.0, {26, 70, 170}, {60, 135, 230}},
            {16.5, {26, 70, 170}, {60, 135, 230}},
            {17.25, {28, 46, 130}, {158, 128, 135}}, // halfway from day to dusk
            {18.0, {30, 22, 90}, {255, 120, 40}},    // the dusk itself
            {18.625, {20, 18, 64}, {136, 71, 48}},   // halfway from dusk to night
        }};
        for (const Hour &hour : known) {
            const auto rows = skyRowColours(hour.hours);
            QVERIFY2(rows.front() == hour.top, qPrintable(QStringLiteral("top row at %1 h").arg(hour.hours)));
            QVERIFY2(rows.back() == hour.bottom, qPrintable(QStringLiteral("bottom row at %1 h").arg(hour.hours)));
        }
        QCOMPARE(skyRowColours(5.625).at(2), (Rgb{81, 43, 82}));
        QCOMPARE(skyRowColours(18.625).at(3), (Rgb{112, 39, 70}));
    }

    void theHourWrapsAroundTheDay()
    {
        QCOMPARE(skyRowColours(12.0 + 24.0), skyRowColours(12.0));
        QCOMPARE(skyRowColours(-6.0), skyRowColours(18.0)); // six hours before midnight
        QCOMPARE(skyRowColours(30.0), skyRowColours(6.0));
    }

    void theDuskIsRedderThanTheDayAtTheBottomOfTheKeyboard()
    {
        const auto day = skyRowColours(12.0);
        const auto dusk = skyRowColours(18.0);
        QVERIFY(dusk.back().red > day.back().red);
        QVERIFY(dusk.back().blue < day.back().blue);
    }

    void thereIsNightnessOnlyAtNightAndWhileTheSkyChanges()
    {
        QCOMPARE(skyNightness(0.0), 1.0);
        QCOMPARE(skyNightness(4.9), 1.0);
        QCOMPARE(skyNightness(5.625), 0.5);
        QCOMPARE(skyNightness(6.25), 0.0);
        QCOMPARE(skyNightness(12.0), 0.0);
        QCOMPARE(skyNightness(18.0), 0.0);
        QCOMPARE(skyNightness(18.625), 0.5);
        QCOMPARE(skyNightness(19.25), 1.0);
        QCOMPARE(skyNightness(23.9), 1.0);
    }

    void aLedIsPlacedInTheRowOfItsLineOfKeysOrOnTheStrip()
    {
        // The y of the measured LEDs (data/devices/galleon-100-sd-led-positions.json): 43, 63/64, 84, 104, 124/125, 146 mm and the strip at 16.8 mm.
        QVERIFY(skyPlaceOf(16.8).strip);
        QVERIFY(!skyPlaceOf(43.0).strip);
        const std::array<std::pair<double, int>, 8> rows{{{43.0, 0}, {63.0, 1}, {64.0, 1}, {84.0, 2}, {104.0, 3}, {124.0, 4}, {125.0, 4}, {146.0, 5}}};
        for (const auto &[y, row] : rows)
            QCOMPARE(skyPlaceOf(y).row, row);
        QCOMPARE(skyPlaceOf(94.0).row, 2); // the lower part of the ISO Enter, between two lines, goes to the upper one
    }

    void aLedFarOutsideTheKeysStaysInTheNearestRow()
    {
        QCOMPARE(skyPlaceOf(500.0).row, SkyRowCount - 1);
        QVERIFY(!skyPlaceOf(500.0).strip);
        QCOMPARE(skyPlaceOf(30.0).row, 0);
        QVERIFY(!skyPlaceOf(30.0).strip); // just under the strip line it is the top row
    }

    // ---- anchored on the sun: the keyframes follow the sunrise and the sunset, with the gaps of the mockup

    void theDefaultDayIsTheScheduleOfTheMockup()
    {
        const SkyDay mockup;
        QCOMPARE(mockup.sunriseHours, 6.25);
        QCOMPARE(mockup.dayLengthHours, 11.75); // the dusk at 18 h
        for (const double hours : {0.0, 5.0, 5.625, 6.25, 7.0, 12.0, 17.25, 18.0, 18.625, 19.25, 23.0})
            QCOMPARE(skyRowColours(hours), skyRowColours(hours, SkyDay{6.25, 11.75}));
    }

    void theKeyframesFollowTheSunriseAndTheSunsetWithTheSameGaps()
    {
        const SkyDay late{8.25, 9.0}; // the sun rises at 8 h 15 and sets at 17 h 15
        QCOMPARE(skyRowColours(8.25, late).front(), (Rgb{34, 30, 92}));  // the dawn is at the sunrise
        QCOMPARE(skyRowColours(9.5, late).front(), (Rgb{26, 70, 170}));  // the day 1 h 15 after it
        QCOMPARE(skyRowColours(15.75, late).front(), (Rgb{26, 70, 170})); // the day until 1 h 30 before the sunset
        QCOMPARE(skyRowColours(17.25, late).front(), (Rgb{30, 22, 90})); // the dusk is at the sunset
        QCOMPARE(skyRowColours(18.5, late).front(), (Rgb{10, 14, 38}));  // the night 1 h 15 after it
        QCOMPARE(skyRowColours(7.0, late).front(), (Rgb{10, 14, 38}));   // the night until 1 h 15 before the sunrise
        QCOMPARE(skyRowColours(7.625, late).front(), (Rgb{22, 22, 65})); // halfway between: the same blend as the mockup
    }

    void aDayThatIsTheMockupMovedAlongGivesTheSameColoursMovedAlong()
    {
        for (const double shift : {-3.0, 1.5, 4.0}) {
            for (const double hours : {1.0, 5.7, 6.25, 7.0, 12.0, 17.4, 18.3, 22.0})
                QCOMPARE(skyRowColours(hours + shift, SkyDay{6.25 + shift, 11.75}), skyRowColours(hours));
        }
    }

    void theSunSettingAfterMidnightKeepsTheKeyframesInOrder()
    {
        // Reykjavik at the summer solstice: sunrise at 2 h 56, a day of 21 h 06, so the sunset is at 0 h 02 of the next day.
        const SkyDay north{2.939, 21.1};
        QCOMPARE(skyRowColours(2.939, north).front(), (Rgb{34, 30, 92}));          // the dawn
        QCOMPARE(skyRowColours(2.939 + 21.1, north).front(), (Rgb{30, 22, 90}));    // the dusk, at 24.04 h
        QCOMPARE(skyRowColours(0.039, north).front(), (Rgb{30, 22, 90}));           // the same instant, on the clock
        const auto atNight = skyRowColours(2.939 + 21.1 + 1.25 * 0.48, north).front(); // the short night: the gaps shrink in proportion
        QVERIFY(atNight.blue < 90);
    }

    void aShortNightAndAShortDayShrinkTheGapsInsteadOfBreakingTheOrder()
    {
        // A night of 2 h (a day of 22 h): the night is there for an instant, at 23 h 00 after the sunrise.
        const SkyDay shortNight{4.0, 22.0};
        QCOMPARE(skyRowColours(4.0 + 23.0, shortNight), skyRowColours(4.0 + 23.0 - 24.0, shortNight));
        QCOMPARE(skyRowColours(4.0 + 23.0, shortNight).front(), (Rgb{10, 14, 38}));
        // A day of 1 h (near the polar night): the day is there for an instant, in the middle of the gaps.
        const SkyDay shortDay{11.0, 1.0};
        QCOMPARE(skyRowColours(11.0, shortDay).front(), (Rgb{34, 30, 92}));
        QCOMPARE(skyRowColours(12.0, shortDay).front(), (Rgb{30, 22, 90}));
        // Every colour of every hour is a colour (no value out of the palette), and the sky has no jump: a thousandth of an hour changes little.
        for (const SkyDay &day : {SkyDay{6.25, 11.75}, shortNight, shortDay, SkyDay{0.0, 12.0}, SkyDay{23.5, 12.0}, SkyDay{2.9, 21.1}}) {
            for (int step = 0; step < 2400; ++step) {
                const double hours = step * 0.01;
                const auto a = skyRowColours(hours, day);
                const auto b = skyRowColours(hours + 0.001, day);
                for (std::size_t row = 0; row < a.size(); ++row) {
                    QVERIFY(std::abs(int(a.at(row).red) - int(b.at(row).red)) <= 2);
                    QVERIFY(std::abs(int(a.at(row).green) - int(b.at(row).green)) <= 2);
                    QVERIFY(std::abs(int(a.at(row).blue) - int(b.at(row).blue)) <= 2);
                }
            }
        }
    }

    void theNightnessFollowsTheSunToo()
    {
        const SkyDay late{8.25, 9.0};
        QCOMPARE(skyNightness(7.0, late), 1.0);
        QCOMPARE(skyNightness(7.625, late), 0.5);
        QCOMPARE(skyNightness(8.25, late), 0.0);
        QCOMPARE(skyNightness(12.0, late), 0.0);
        QCOMPARE(skyNightness(17.25, late), 0.0);
        QCOMPARE(skyNightness(17.875, late), 0.5);
        QCOMPARE(skyNightness(18.5, late), 1.0);
        QCOMPARE(skyNightness(2.0, late), 1.0);
    }

    void theSkyOfADayOfTheSunIsItsRealTimesAndAPolarDayFallsBackToTheMockup()
    {
        const SunDay paris = sunDay(48.8566, 2.3522, 2026, 6, 21, 2.0);
        const SkyDay parisSky = skyDayFrom(paris);
        QCOMPARE(parisSky.sunriseHours, paris.sunriseHours);
        QCOMPARE(parisSky.dayLengthHours, paris.dayLengthHours);
        QCOMPARE(skyRowColours(paris.sunriseHours, parisSky).front(), (Rgb{34, 30, 92})); // dawn at the real sunrise
        QCOMPARE(skyRowColours(paris.sunsetHours, parisSky).front(), (Rgb{30, 22, 90}));  // dusk at the real sunset
        // No sunrise or no sunset: the schedule of the mockup, whatever the other fields hold.
        QCOMPARE(skyDayFrom(sunDay(69.6496, 18.956, 2026, 12, 21, 1.0)), SkyDay{});
        QCOMPARE(skyDayFrom(sunDay(69.6496, 18.956, 2026, 6, 21, 2.0)), SkyDay{});
    }

    void theStripIsTheTopRowMixedFortyPercentTowardsBlack()
    {
        QCOMPARE(skyStripColour({100, 50, 255}), (Rgb{60, 30, 153})); // 60 % of each channel, rounded
        QCOMPARE(skyStripColour({0, 0, 0}), (Rgb{0, 0, 0}));
    }
};

QTEST_GUILESS_MAIN(SkyPaletteTest)
#include "test_skypalette.moc"
