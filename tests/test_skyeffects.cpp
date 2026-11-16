// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: LGPL-3.0-or-later

// The shimmer by day and the stars by night of the sky: pure and stateless, so the same time always gives the same picture. The numbers are those of
// the mockup of Aaryush1 (the shimmer takes a key up to 10 % brighter, nine stars drift at 0.25 to 0.5 keys a second, the stars are not on the top
// row, the twinkle goes to white at 0.85).

#include <galleon/keyboard/SkyEffects.h>

#include <QTest>

#include <array>
#include <cmath>
#include <set>
#include <vector>

using namespace galleon::keyboard;

namespace {

// A small keyboard: six rows of twelve keys, 20 mm apart, rows at 43, 63, 84, 104, 125 and 146 mm (the heights of the real one), and the strip.
std::vector<LedPosition> keyboard()
{
    const std::array<double, 6> heights{43.0, 63.0, 84.0, 104.0, 125.0, 146.0};
    std::vector<LedPosition> positions;
    std::uint16_t led = 0;
    for (const double y : heights) {
        for (int i = 0; i < 12; ++i)
            positions.push_back({led++, 40.0 + 20.0 * i, y});
    }
    for (int i = 0; i < 7; ++i) // the light strip, above the rows
        positions.push_back({static_cast<std::uint16_t>(100 + i), 60.0 + 30.0 * i, 16.8});
    return positions;
}

// A keyboard like the real one: rows of different lengths, and big keys that leave a gap (row 3 stops at 200 mm; row 5 has a space bar between two keys 140 mm apart).
std::vector<LedPosition> uneven()
{
    const std::array<double, 6> heights{43.0, 63.0, 84.0, 104.0, 125.0, 146.0};
    const std::array<std::vector<double>, 6> xs{{{40, 60, 80, 100, 120, 140, 160, 180, 200, 220, 240, 260},
                                                 {40, 60, 80, 100, 120, 140, 160, 180, 200, 220, 240, 260},
                                                 {50, 70, 90, 110, 130, 150, 170, 190, 210, 230, 250, 270},
                                                 {60, 80, 100, 120, 140, 160, 180, 200},
                                                 {50, 70, 90, 110, 130, 150, 170, 190, 210, 230, 250},
                                                 {40, 60, 80, 220, 240, 260}}};
    std::vector<LedPosition> positions;
    std::uint16_t led = 0;
    for (std::size_t row = 0; row < heights.size(); ++row) {
        for (const double x : xs.at(row))
            positions.push_back({led++, x, heights.at(row)});
    }
    return positions;
}

using Colours = std::array<Rgb, LiveLedCount>;

Colours flat(const Rgb &colour)
{
    Colours colours{};
    colours.fill(colour);
    return colours;
}

Colours animated(const Rgb &base, double seconds, double nightness)
{
    Colours colours = flat(base);
    animateSky(keyboard(), colours, seconds, nightness);
    return colours;
}

bool isStrip(std::uint16_t led)
{
    return led >= 100 && led <= 106;
}

} // namespace

class SkyEffectsTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void theSameTimeGivesTheSamePicture()
    {
        for (const double nightness : {0.0, 0.5, 1.0}) {
            for (const double seconds : {0.0, 3.3, 41.7, 3600.2})
                QVERIFY(animated({60, 135, 230}, seconds, nightness) == animated({60, 135, 230}, seconds, nightness));
        }
    }

    void theSkyOnlyChangesFromOneStepOfAnEighthOfASecondToTheNext()
    {
        const double step = SkyEffectStep;
        QVERIFY(animated({60, 135, 230}, 5.0, 0.0) == animated({60, 135, 230}, 5.0 + step * 0.9, 0.0)); // the same step
        QVERIFY(animated({60, 135, 230}, 5.0, 0.0) != animated({60, 135, 230}, 5.0 + step * 3.0, 0.0)); // a later step
    }

    void byDayTheKeysShimmerBrighterUpToTenPercentAndNoStarShows()
    {
        const Rgb base{60, 135, 230};
        bool anyBrighter = false;
        for (const double seconds : {0.0, 2.0, 7.0, 13.0, 29.0}) {
            const Colours colours = animated(base, seconds, 0.0);
            for (const LedPosition &p : keyboard()) {
                const Rgb &c = colours.at(p.led);
                if (isStrip(p.led)) {
                    QCOMPARE(c, base); // the strip does not shimmer
                    continue;
                }
                QVERIFY(c.red >= base.red && c.green >= base.green && c.blue >= base.blue); // never darker
                QVERIFY(c.red <= base.red * 1.1 + 1 && c.green <= base.green * 1.1 + 1 && c.blue <= 255); // at most 10 % more (the mockup's 1.2 mixed to 0.5)
                anyBrighter = anyBrighter || c != base;
            }
        }
        QVERIFY(anyBrighter);
    }

    void aKeyShimmersOutOfStepWithItsNeighbour()
    {
        const Colours colours = animated({60, 135, 230}, 4.0, 0.0);
        std::set<int> reds;
        for (std::uint16_t led = 12; led < 24; ++led) // one row
            reds.insert(colours.at(led).blue);
        QVERIFY(reds.size() > 3); // not all the same: each key has its own phase
    }

    void theShimmerFadesAwayAtNightAndTheStarsComeIn()
    {
        const Rgb night{12, 16, 44};
        // At full night no key shimmers: the keys are the colour of the night, except where a star or a twinkle is.
        int changed = 0;
        for (const double seconds : {10.0, 55.0, 120.0}) {
            const Colours colours = animated(night, seconds, 1.0);
            for (const LedPosition &p : keyboard()) {
                if (colours.at(p.led) != night)
                    ++changed;
            }
        }
        QVERIFY(changed > 0); // stars (and twinkles) are there
        // They are few: nine stars light at most two keys each, and twinkles are rare.
        const Colours colours = animated(night, 10.0, 1.0);
        int lit = 0;
        for (const LedPosition &p : keyboard()) {
            if (colours.at(p.led) != night)
                ++lit;
        }
        QVERIFY2(lit <= 2 * SkyStarCount + 8, qPrintable(QStringLiteral("%1 keys differ").arg(lit)));
    }

    void theStarsAreNotOnTheTopRowNorOnTheStrip()
    {
        const Rgb night{12, 16, 44};
        for (int step = 0; step < 4000; ++step) { // over 8 minutes
            const Colours colours = animated(night, step * 0.125, 1.0);
            for (std::uint16_t led = 0; led < 12; ++led)
                QCOMPARE(colours.at(led), night); // the top row: nothing, even the twinkles
            for (std::uint16_t led = 100; led < 107; ++led)
                QCOMPARE(colours.at(led), night);
        }
    }

    void aStarIsPaleAndBrightensTheKeysOfItsRowAndMovesOverTheMinutes()
    {
        const Rgb night{12, 16, 44};
        std::set<std::uint16_t> everLit;
        for (int step = 0; step < 4800; step += 8) { // every second for 10 minutes
            const Colours colours = animated(night, step * 0.125, 1.0);
            for (const LedPosition &p : keyboard()) {
                const Rgb &c = colours.at(p.led);
                if (c != night) {
                    everLit.insert(p.led);
                    QVERIFY(c.blue >= night.blue); // towards pale blue-white: never darker
                    QVERIFY(c.red >= night.red);
                }
            }
        }
        // Over ten minutes the stars have gone over most of the keys of the five rows (60 keys), not stayed in one place.
        QVERIFY2(everLit.size() > 30, qPrintable(QStringLiteral("%1 keys lit over ten minutes").arg(everLit.size())));
    }

    void theNightSkyIsAGentleMotionNotAFlash()
    {
        // Measured over ten minutes in steps of an eighth of a second: on average about 17 keys are off the night colour at a step (the keys of nine
        // stars, which light two keys each, and the twinkles), and about as many change from a step to the next as the stars drift.
        const Rgb night{12, 16, 44};
        long off = 0;
        long changed = 0;
        Colours previous = animated(night, 0.0, 1.0);
        for (int step = 1; step < 4800; ++step) {
            const Colours now = animated(night, step * SkyEffectStep, 1.0);
            for (const LedPosition &p : keyboard()) {
                off += now.at(p.led) != night ? 1 : 0;
                changed += now.at(p.led) != previous.at(p.led) ? 1 : 0;
            }
            previous = now;
        }
        const double offPerStep = static_cast<double>(off) / 4799.0;
        const double changedPerStep = static_cast<double>(changed) / 4799.0;
        QVERIFY2(offPerStep > 8.0 && offPerStep < 30.0, qPrintable(QStringLiteral("%1 keys off the night colour per step").arg(offPerStep)));
        QVERIFY2(changedPerStep > 5.0 && changedPerStep < 35.0, qPrintable(QStringLiteral("%1 keys changing per step").arg(changedPerStep)));
    }

    void twinklesGoToWhiteAtTheRateOfTheMockup()
    {
        // A twinkle goes 85 % of the way to white: red above 210 from a night of 12, which a star (at most 80 % of a pale blue-white) never reaches.
        // The mockup draws about 60 pictures a second with a chance of 0.0012 for each of the 55 keys of the five rows, so about 4 twinkles a
        // second, which is about 0.5 a step of an eighth of a second; over 800 steps (100 seconds) that is about 430 twinkles (each lasts up to 3 steps).
        const Rgb night{12, 16, 44};
        int peaks = 0;
        for (int step = 0; step < 800; ++step) {
            const Colours colours = animated(night, step * SkyEffectStep, 1.0);
            for (const LedPosition &p : keyboard()) {
                if (p.led >= 12 && !isStrip(p.led) && colours.at(p.led).red > 210)
                    ++peaks;
            }
        }
        QVERIFY2(peaks > 200 && peaks < 700, qPrintable(QStringLiteral("%1 twinkle peaks in 100 seconds").arg(peaks)));
    }

    void everyStarIsSeenAllItsWalkEvenOnRowsOfDifferentLengthsAndAcrossGaps()
    {
        // The nine stars never vanish in the middle of a row: at every second of ten minutes at least eight keys are lit by the stars (the walk of a star
        // fades at its two ends, and two stars can share a key, so a few seconds have one less).
        const auto positions = uneven();
        int fewest = 99;
        for (int second = 0; second < 600; ++second) {
            const auto light = starLights(positions, second);
            int lit = 0;
            for (const LedPosition &p : positions)
                lit += light.at(p.led) > 0.0 ? 1 : 0;
            fewest = std::min(fewest, lit);
        }
        QVERIFY2(fewest >= 8, qPrintable(QStringLiteral("only %1 keys lit by the stars at the worst second").arg(fewest)));
    }

    void aStarCrossesTheGapOfABigKeyByLightingItsTwoNeighbours()
    {
        // In row 5 the keys at 80 and 220 mm are 140 mm apart (the space bar between them is a key of its own, not in this list): somewhere in ten minutes a
        // star is between them, and then both light up, in proportion.
        const auto positions = uneven();
        std::uint16_t first = 0;
        std::uint16_t second = 0;
        for (const LedPosition &p : positions) {
            if (p.y == 146.0 && p.x == 80.0)
                first = p.led;
            if (p.y == 146.0 && p.x == 220.0)
                second = p.led;
        }
        bool both = false;
        for (int step = 0; step < 6000 && !both; ++step) {
            const auto light = starLights(positions, step * 0.5);
            both = light.at(first) > 0.1 && light.at(second) > 0.1 && std::abs(light.at(first) - light.at(second)) < 0.4;
        }
        QVERIFY2(both, "no star was ever seen crossing the gap");
    }

    void aStarAppearsAndVanishesGentlyAndNeverJumpsInBrightness()
    {
        // Between two steps of an eighth of a second, no key changes by more than a small share of the light of a star: neither a star that drifts, nor one
        // that begins a new walk on another row (it has faded out, and fades in), makes a pop.
        const auto positions = uneven();
        double largest = 0.0;
        auto previous = starLights(positions, 0.0);
        for (int step = 1; step < 9600; ++step) { // twenty minutes
            const auto now = starLights(positions, step * SkyEffectStep);
            for (const LedPosition &p : positions)
                largest = std::max(largest, std::abs(now.at(p.led) - previous.at(p.led)));
            previous = now;
        }
        QVERIFY2(largest < 0.12, qPrintable(QStringLiteral("a key changed by %1 in one step").arg(largest)));
    }

    void theStarsWalkEveryRowFromItsFirstKeyToItsLastOne()
    {
        // Over three hours every key of rows 1 to 5, the first and the last of each row included, is lit well by a star at some moment; the top row never.
        const auto positions = uneven();
        std::array<double, LiveLedCount> strongest{};
        for (int second = 0; second < 10800; second += 2) {
            const auto light = starLights(positions, second);
            for (const LedPosition &p : positions)
                strongest.at(p.led) = std::max(strongest.at(p.led), light.at(p.led));
        }
        for (const LedPosition &p : positions) {
            if (p.y == 43.0)
                QCOMPARE(strongest.at(p.led), 0.0); // not the top row
            else
                QVERIFY2(strongest.at(p.led) > 0.3, qPrintable(QStringLiteral("key at %1 mm, row of %2 mm, never well lit").arg(p.x).arg(p.y)));
        }
    }

    void withoutPositionsNothingIsDone()
    {
        Colours colours = flat({1, 2, 3});
        animateSky({}, colours, 5.0, 1.0);
        QVERIFY(colours == flat({1, 2, 3}));
    }

    void theTwinkleDoesNotDependOnTheOrderOfTheKeysGivenToIt()
    {
        auto reversed = keyboard();
        std::ranges::reverse(reversed);
        Colours a = flat({12, 16, 44});
        Colours b = flat({12, 16, 44});
        animateSky(keyboard(), a, 33.0, 1.0);
        animateSky(reversed, b, 33.0, 1.0);
        QVERIFY(a == b);
    }
};

QTEST_GUILESS_MAIN(SkyEffectsTest)
#include "test_skyeffects.moc"
