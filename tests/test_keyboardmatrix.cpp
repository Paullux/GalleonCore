// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: LGPL-3.0-or-later

// The green rain on the LEDs of the keyboard: pure and stateless, the same picture as the screen and the keys of the Stream Deck (a head that falls,
// almost white, a trail above it that goes from green to black, in a loop, each column at its own speed).

#include <galleon/keyboard/MatrixRain.h>

#include <QTest>

#include <array>
#include <map>
#include <ranges>
#include <set>
#include <vector>

using namespace galleon::keyboard;

namespace {

constexpr Rgb Head{216, 255, 224};

// A keyboard of 12 columns (20.5 mm) and six rows (43, 63, 84, 104, 125, 146 mm), and a light strip of seven LEDs above them.
std::vector<LedPosition> keyboard()
{
    const std::array<double, 6> heights{43.0, 63.0, 84.0, 104.0, 125.0, 146.0};
    std::vector<LedPosition> positions;
    std::uint16_t led = 0;
    for (const double y : heights) {
        for (int i = 0; i < 12; ++i)
            positions.push_back({led++, 40.0 + 20.5 * i, y});
    }
    for (int i = 0; i < 7; ++i)
        positions.push_back({static_cast<std::uint16_t>(100 + i), 50.0 + 30.0 * i, 16.8});
    return positions;
}

bool dark(const Rgb &c)
{
    return c.red == 0 && c.green == 0 && c.blue == 0;
}

// The line of a LED: the strip is line 0, the rows of keys are lines 1 to 6.
int lineOf(const LedPosition &p)
{
    return p.y < 30.0 ? 0 : 1 + static_cast<int>(std::lround((p.y - 43.0) / 20.5));
}

int columnOf(const LedPosition &p)
{
    return static_cast<int>(std::lround((p.x - 40.0) / 20.5));
}

} // namespace

class KeyboardMatrixTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void theSameMomentGivesTheSamePictureAndItChangesFromOneStepToTheNext()
    {
        const auto positions = keyboard();
        const double begin = 136 * MatrixTickSeconds + 0.001; // just after the start of a step
        QVERIFY(matrixRainColours(positions, begin) == matrixRainColours(positions, begin));
        QVERIFY(matrixRainColours(positions, begin) == matrixRainColours(positions, begin + MatrixTickSeconds * 0.8)); // the same step
        bool changes = false;
        for (int step = 0; step < 40 && !changes; ++step)
            changes = matrixRainColours(positions, step * MatrixTickSeconds) != matrixRainColours(positions, (step + 1) * MatrixTickSeconds);
        QVERIFY(changes);
    }

    void theColoursAreTheOnesOfTheScreenAHeadAlmostWhiteAndGreensDownToBlack()
    {
        const auto positions = keyboard();
        int heads = 0;
        int trails = 0;
        for (int step = 0; step < 400; ++step) {
            const auto colours = matrixRainColours(positions, step * MatrixTickSeconds);
            for (const LedPosition &p : positions) {
                const Rgb &c = colours.at(p.led);
                if (dark(c))
                    continue;
                if (c == Head) {
                    ++heads;
                } else {
                    ++trails;
                    QCOMPARE(int(c.red), 0);               // pure green: the trail of the letters
                    QVERIFY(c.green > 0 && c.green <= 212);
                    QVERIFY(c.blue <= c.green);            // green, not white
                }
            }
        }
        QVERIFY(heads > 100);
        QVERIFY(trails > heads); // each head has a trail
    }

    void eachColumnHasAtMostOneHeadAndItsTrailIsAboveItGettingDarker()
    {
        const auto positions = keyboard();
        // The head of each column, by colour, and the trail above it.
        for (int step = 0; step < 300; ++step) {
            const auto colours = matrixRainColours(positions, step * MatrixTickSeconds);
            std::map<int, std::vector<const LedPosition *>> columns;
            for (const LedPosition &p : positions)
                columns[columnOf(p)].push_back(&p);
            for (auto &[column, leds] : columns) {
                std::ranges::sort(leds, [](const LedPosition *a, const LedPosition *b) { return lineOf(*a) < lineOf(*b); });
                int heads = 0;
                int headLine = -1;
                for (const LedPosition *p : leds) {
                    if (colours.at(p->led) == Head) {
                        ++heads;
                        headLine = lineOf(*p);
                    }
                }
                QVERIFY2(heads <= 1, qPrintable(QStringLiteral("column %1 has %2 heads").arg(column).arg(heads)));
                if (heads == 0)
                    continue;
                // Below the head: dark. Above it: green, the darker the farther from the head.
                int previousGreen = 256;
                for (const LedPosition *p : leds | std::views::reverse) {
                    const int line = lineOf(*p);
                    const Rgb &c = colours.at(p->led);
                    if (line > headLine) {
                        QVERIFY(dark(c));
                    } else if (line < headLine) {
                        QVERIFY(c.green <= previousGreen);
                        previousGreen = c.green == 0 ? previousGreen : c.green;
                    }
                }
            }
        }
    }

    void theHeadsFallDownTheColumnsAndStartAgainAtTheTop()
    {
        const auto positions = keyboard();
        // Follow one column (the fourth): the line of its head grows with time, and goes back to the top when the cycle ends.
        int previous = -100;
        int falls = 0;
        int restarts = 0;
        for (int step = 0; step < 600; ++step) {
            const auto colours = matrixRainColours(positions, step * MatrixTickSeconds);
            int headLine = -100;
            for (const LedPosition &p : positions) {
                if (columnOf(p) == 3 && colours.at(p.led) == Head)
                    headLine = lineOf(p);
            }
            if (headLine == -100)
                continue;
            if (previous != -100) {
                if (headLine > previous)
                    ++falls;
                else if (headLine < previous)
                    ++restarts;
            }
            previous = headLine;
        }
        QVERIFY2(falls > 5, "the head never fell");
        QVERIFY2(restarts >= 2, "the rain never began again");
        QVERIFY2(falls > restarts, "it goes down far more often than it starts over");
    }

    void everyKeyAndTheStripAreLitSomeTimeAndTheKeyboardIsMostlyDark()
    {
        const auto positions = keyboard();
        std::set<std::uint16_t> everLit;
        long litTotal = 0;
        long cells = 0;
        for (int step = 0; step < 1200; ++step) { // two minutes
            const auto colours = matrixRainColours(positions, step * MatrixTickSeconds);
            for (const LedPosition &p : positions) {
                ++cells;
                if (!dark(colours.at(p.led))) {
                    everLit.insert(p.led);
                    ++litTotal;
                }
            }
        }
        QCOMPARE(everLit.size(), positions.size()); // the top row and the strip too
        const double fraction = static_cast<double>(litTotal) / static_cast<double>(cells);
        QVERIFY2(fraction > 0.15 && fraction < 0.6, qPrintable(QStringLiteral("%1 of the keys lit on average").arg(fraction)));
    }

    void theColumnsFallAtDifferentSpeedsAndFromDifferentPlaces()
    {
        const auto positions = keyboard();
        const auto colours = matrixRainColours(positions, 7.0);
        std::set<int> headLines;
        for (const LedPosition &p : positions) {
            if (colours.at(p.led) == Head)
                headLines.insert(lineOf(p));
        }
        QVERIFY(headLines.size() >= 3); // the heads are not all on the same line
        // Over ten seconds the columns do not move in step: the number of lit keys changes.
        std::set<int> counts;
        for (int step = 0; step < 100; ++step) {
            const auto c = matrixRainColours(positions, step * MatrixTickSeconds);
            int lit = 0;
            for (const LedPosition &p : positions)
                lit += dark(c.at(p.led)) ? 0 : 1;
            counts.insert(lit);
        }
        QVERIFY(counts.size() >= 4);
    }

    void withoutPositionsOrBeforeTheBeginningEverythingIsDark()
    {
        const auto none = matrixRainColours({}, 5.0);
        for (const Rgb &c : none)
            QVERIFY(dark(c));
        const auto before = matrixRainColours(keyboard(), -1.0);
        for (const Rgb &c : before)
            QVERIFY(dark(c));
        // A LED with no position stays dark: the keyboard has 146 LEDs and only the ones that light something have a place.
        const auto colours = matrixRainColours(keyboard(), 7.0);
        for (std::size_t led = 90; led < 100; ++led)
            QVERIFY(dark(colours.at(led)));
    }
};

QTEST_GUILESS_MAIN(KeyboardMatrixTest)
#include "test_keyboardmatrix.moc"
