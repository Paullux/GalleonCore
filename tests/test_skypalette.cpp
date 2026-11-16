// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: LGPL-3.0-or-later

// The sky that follows the hour: the palette of the mockup of Aaryush1 blended between its keyframes. The expected colours were computed
// apart, in Python, with the formula of the mockup (each channel `round(from + (to - from) * fraction)` between two keyframes).

#include <galleon/keyboard/SkyPalette.h>

#include <QTest>

#include <array>
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

    void theStripIsTheTopRowMixedFortyPercentTowardsBlack()
    {
        QCOMPARE(skyStripColour({100, 50, 255}), (Rgb{60, 30, 153})); // 60 % of each channel, rounded
        QCOMPARE(skyStripColour({0, 0, 0}), (Rgb{0, 0, 0}));
    }
};

QTEST_GUILESS_MAIN(SkyPaletteTest)
#include "test_skypalette.moc"
