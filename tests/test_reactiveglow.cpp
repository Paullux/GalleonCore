// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

// The minimal reactive effect, pure engine: what it keeps (a LED and a time, bounded, for a limited time) and how the glow is drawn over a frame.
// Nothing here reads a key press; these tests hold the limits that keep the board from becoming a record of what was typed.
#include <galleon/keyboard/ReactiveGlow.h>

#include <QTest>

#include <cmath>
#include <limits>

using namespace galleon::keyboard;

namespace {

constexpr Rgb Cyan{0, 200, 255};
constexpr Rgb Amber{255, 160, 64};

LiveFrame frameOf(Rgb colour)
{
    LiveFrame frame{};
    frame[0] = 0x12;
    for (std::size_t led = 0; led < LiveLedCount; ++led) {
        frame.at(2 + 3 * led) = colour.red;
        frame.at(3 + 3 * led) = colour.green;
        frame.at(4 + 3 * led) = colour.blue;
    }
    return frame;
}

Rgb pixel(const LiveFrame &frame, std::size_t led)
{
    return {frame.at(2 + 3 * led), frame.at(3 + 3 * led), frame.at(4 + 3 * led)};
}

double shareOf(const std::vector<LedShare> &glow, int led)
{
    for (const LedShare &entry : glow) {
        if (entry.led == led)
            return entry.share;
    }
    return -1;
}

} // namespace

class TestReactiveGlow : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void aGlowIsFullAtTheTouchAndGoneAtTheEndOfItsFade()
    {
        QCOMPARE(glowShare(0.0, 1.5), 1.0);
        QCOMPARE(glowShare(1.5, 1.5), 0.0);
        QCOMPARE(glowShare(9.0, 1.5), 0.0);
        QVERIFY(std::fabs(glowShare(0.75, 1.5) - 0.25) < 1e-12); // the square of what is left of the time
    }

    void aGlowOnlyEverFalls()
    {
        double before = 1.0;
        for (int step = 1; step <= 150; ++step) {
            const double share = glowShare(step * 0.01, 1.5);
            QVERIFY(share <= before);
            QVERIFY(share >= 0.0);
            before = share;
        }
    }

    void aNegativeOrNotANumberAgeCountsAsJustTouched()
    {
        QCOMPARE(glowShare(-3.0, 1.5), 1.0);
        QCOMPARE(glowShare(std::nan(""), 1.5), 1.0);
    }

    void aFadeThatIsNotValidFallsBackToTheDefault()
    {
        QCOMPARE(glowShare(0.6, 0.0), glowShare(0.6, DefaultGlowFadeSeconds));
        QCOMPARE(glowShare(0.6, 99.0), glowShare(0.6, DefaultGlowFadeSeconds));
        QCOMPARE(glowShare(0.6, std::nan("")), glowShare(0.6, DefaultGlowFadeSeconds));
    }

    void aFadeIsBetweenTwoTenthsAndFiveSeconds_data()
    {
        QTest::addColumn<double>("seconds");
        QTest::addColumn<bool>("valid");
        QTest::newRow("shortest") << MinGlowFadeSeconds << true;
        QTest::newRow("default") << DefaultGlowFadeSeconds << true;
        QTest::newRow("longest") << MaxGlowFadeSeconds << true;
        QTest::newRow("too short") << 0.19 << false;
        QTest::newRow("too long") << 5.01 << false;
        QTest::newRow("zero") << 0.0 << false;
        QTest::newRow("negative") << -1.0 << false;
        QTest::newRow("not a number") << std::nan("") << false;
        QTest::newRow("infinite") << std::numeric_limits<double>::infinity() << false;
    }
    void aFadeIsBetweenTwoTenthsAndFiveSeconds()
    {
        QFETCH(const double, seconds);
        QFETCH(const bool, valid);
        QCOMPARE(isValidGlowFade(seconds), valid);
    }

    void aKeyTouchedGlowsAndIsGoneWhenItsFadeIsOver()
    {
        GlowBoard board;
        QVERIFY(board.touch(0x3A, 10.0));
        QCOMPARE(shareOf(board.shares(10.0, 1.5), 0x3A), 1.0);
        QVERIFY(shareOf(board.shares(10.5, 1.5), 0x3A) > 0.0);
        QCOMPARE(shareOf(board.shares(11.5, 1.5), 0x3A), -1.0);
        board.expire(11.5, 1.5);
        QVERIFY(board.empty());
    }

    void touchingAGlowingKeyAgainStartsItsGlowAnewWithoutAddingAnEntry()
    {
        GlowBoard board;
        QVERIFY(board.touch(4, 10.0));
        QVERIFY(board.touch(4, 10.9));
        QCOMPARE(board.size(), std::size_t{1});
        QCOMPARE(shareOf(board.shares(10.9, 1.5), 4), 1.0);
    }

    void aLedBeyondTheFrameOrATimeThatIsNotANumberChangesNothing()
    {
        GlowBoard board;
        QVERIFY(board.touch(4, 10.0));
        QVERIFY(!board.touch(static_cast<std::uint16_t>(LiveLedCount), 10.0));
        QVERIFY(!board.touch(0xFFFF, 10.0));
        QVERIFY(!board.touch(5, std::nan("")));
        QVERIFY(!board.touch(5, std::numeric_limits<double>::infinity()));
        QCOMPARE(board.size(), std::size_t{1});
    }

    void theBoardNeverHoldsMoreThanItsLimitAndDropsTheOldest()
    {
        GlowBoard board;
        for (std::size_t led = 0; led < 3 * MaxGlowing / 2; ++led)
            QVERIFY(board.touch(static_cast<std::uint16_t>(led), 10.0 + static_cast<double>(led) * 0.001));
        QCOMPARE(board.size(), MaxGlowing);
        const auto glow = board.shares(10.1, 5.0);
        QCOMPARE(shareOf(glow, 0), -1.0); // the first one was dropped
        QVERIFY(shareOf(glow, static_cast<int>(3 * MaxGlowing / 2) - 1) > 0.0); // the last one is kept
    }

    void aKeyHeldDownAndTouchedForeverStaysOneEntry()
    {
        GlowBoard board;
        for (int step = 0; step < 100000; ++step)
            QVERIFY(board.touch(0x2C, 10.0 + step * 0.01));
        QCOMPARE(board.size(), std::size_t{1});
    }

    void aFadeThatIsNotValidDoesNotKeepAGlowForEver()
    {
        for (const double fade : {99.0, 0.0, -1.0, std::nan(""), std::numeric_limits<double>::infinity()}) {
            GlowBoard board;
            QVERIFY(board.touch(7, 10.0));
            board.expire(10.0 + MaxGlowFadeSeconds, fade);
            QVERIFY(board.empty());
        }
    }

    void expireKeepsTheGlowsThatAreNotOver()
    {
        GlowBoard board;
        QVERIFY(board.touch(1, 10.0));
        QVERIFY(board.touch(2, 11.0));
        board.expire(11.6, 1.5);
        QCOMPARE(board.size(), std::size_t{1});
        QVERIFY(shareOf(board.shares(11.6, 1.5), 2) > 0.0);
    }

    void theGlowToDrawIsByIncreasingLedWithOneEntryEach()
    {
        GlowBoard board;
        QVERIFY(board.touch(90, 10.0));
        QVERIFY(board.touch(5, 10.1));
        QVERIFY(board.touch(40, 10.2));
        QVERIFY(board.touch(5, 10.3));
        const auto glow = board.shares(10.3, 1.5);
        QCOMPARE(glow.size(), std::size_t{3});
        QCOMPARE(int(glow[0].led), 5);
        QCOMPARE(int(glow[1].led), 40);
        QCOMPARE(int(glow[2].led), 90);
    }

    void clearForgetsEverything()
    {
        GlowBoard board;
        QVERIFY(board.touch(1, 10.0));
        QVERIFY(board.touch(2, 10.0));
        board.clear();
        QVERIFY(board.empty());
        QVERIFY(board.shares(10.0, 1.5).empty());
    }

    void aFullGlowShowsTheColourAndAFadedOneGivesTheFramesBack()
    {
        const LiveFrame base = frameOf({10, 20, 30});
        const std::vector<LedShare> glow{{3, 1.0}, {4, 0.5}, {5, 0.0}};
        const LiveFrame lit = withGlow(base, glow, Cyan, LevelFull);
        QCOMPARE(pixel(lit, 3), Cyan);
        QCOMPARE(pixel(lit, 4), (Rgb{5, 110, 143})); // the middle between {10,20,30} and {0,200,255}, rounded
        QCOMPARE(pixel(lit, 5), (Rgb{10, 20, 30}));
        QCOMPARE(pixel(lit, 6), (Rgb{10, 20, 30})); // the others are untouched
    }

    void theGlowIsAsDimAsTheKeyboardIsAskedToBeAndBlackInTheNight()
    {
        const LiveFrame base = frameOf({0, 0, 0});
        const std::vector<LedShare> glow{{3, 1.0}};
        QCOMPARE(pixel(withGlow(base, glow, Amber, LevelFull / 2), 3), (Rgb{128, 80, 32}));
        QCOMPARE(pixel(withGlow(base, glow, Amber, 0), 3), (Rgb{0, 0, 0}));
        QCOMPARE(pixel(withGlow(base, glow, Amber, -50), 3), (Rgb{0, 0, 0}));
        QCOMPARE(pixel(withGlow(base, glow, Amber, 5000), 3), Amber);
    }

    void aGlowOnALedBeyondTheFrameOrWithAShareNotANumberIsIgnored()
    {
        const LiveFrame base = frameOf({10, 20, 30});
        const std::vector<LedShare> glow{{static_cast<std::uint16_t>(LiveLedCount), 1.0}, {0xFFFF, 1.0}, {2, std::nan("")}};
        QCOMPARE(withGlow(base, glow, Cyan, LevelFull), base);
    }

    void aShareBeyondOneOrBelowZeroIsHeldWithinTheRange()
    {
        const LiveFrame base = frameOf({10, 20, 30});
        QCOMPARE(pixel(withGlow(base, std::vector<LedShare>{{1, 7.0}}, Cyan, LevelFull), 1), Cyan);
        QCOMPARE(pixel(withGlow(base, std::vector<LedShare>{{1, -3.0}}, Cyan, LevelFull), 1), (Rgb{10, 20, 30}));
    }

    void withoutAGlowTheFrameIsTheSame()
    {
        const LiveFrame base = frameOf({10, 20, 30});
        QCOMPARE(withGlow(base, {}, Cyan, LevelFull), base);
    }
};

QTEST_APPLESS_MAIN(TestReactiveGlow)
#include "test_reactiveglow.moc"
