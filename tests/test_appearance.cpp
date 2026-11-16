// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: LGPL-3.0-or-later

// The appearance of the live lighting (a base, retouches, a level) and its rendering to one colour per LED. Pure: the positions and the time
// are arguments, nothing is sent. The expected colours of the rainbow come from an independent calculation (Python's colorsys).

#include <galleon/keyboard/Appearance.h>

#include <QTest>

#include <array>
#include <vector>

using namespace galleon::keyboard;

namespace {

// Five LEDs along the keyboard, 0 mm to 40 mm; LED 9 has no position (it lights nothing).
std::vector<LedPosition> fivePositions()
{
    return {{0, 0, 10}, {1, 10, 10}, {2, 20, 10}, {3, 30, 10}, {4, 40, 10}};
}

std::array<std::uint8_t, 3> colourOf(const LiveScene &scene, std::size_t led)
{
    return {scene.colours.at(3 * led), scene.colours.at(3 * led + 1), scene.colours.at(3 * led + 2)};
}

LiveScene render(const Appearance &appearance, double seconds = 0.0)
{
    return renderAppearance(appearance, fivePositions(), seconds).value_or(LiveScene{});
}

using Bytes = std::array<std::uint8_t, 3>;

} // namespace

class AppearanceTest : public QObject
{
    Q_OBJECT

private slots:
    void aUniformBaseGivesEveryLedThatLightsSomethingOneColour()
    {
        Appearance appearance;
        appearance.colour = {10, 20, 30};
        const LiveScene scene = render(appearance);
        for (std::size_t led = 0; led < 5; ++led)
            QCOMPARE(colourOf(scene, led), (Bytes{10, 20, 30}));
        QCOMPARE(colourOf(scene, 9), (Bytes{0, 0, 0})); // no position: dark
    }

    void theSkyGivesEachRowOfKeysTheColourOfTheHourAndTheStripTheTopRowDimmed()
    {
        // A strip LED (y = 16.8), a LED of the top row (43), one of the third row (84) and one of the last row (146).
        const std::vector<LedPosition> sky{{0, 0, 16.8}, {1, 10, 43}, {2, 20, 84}, {3, 30, 146}};
        Appearance appearance;
        appearance.base = BaseType::Sky;
        const auto day = renderAppearance(appearance, sky, 0.0, 12.0).value_or(LiveScene{});
        QCOMPARE(colourOf(day, 1), (Bytes{26, 70, 170}));  // the top row at noon
        QCOMPARE(colourOf(day, 2), (Bytes{36, 95, 200}));  // the third row
        QCOMPARE(colourOf(day, 3), (Bytes{60, 135, 230})); // the last row
        QCOMPARE(colourOf(day, 0), (Bytes{16, 42, 102}));  // the strip: the top row 40 % of the way to black
        const auto dusk = renderAppearance(appearance, sky, 0.0, 18.0).value_or(LiveScene{});
        QCOMPARE(colourOf(dusk, 1), (Bytes{30, 22, 90}));
        QCOMPARE(colourOf(dusk, 3), (Bytes{255, 120, 40}));
    }

    void theSkyIsAppliedBeforeTheRetouchesAndTheLevel()
    {
        const std::vector<LedPosition> sky{{1, 10, 43}, {3, 30, 146}};
        Appearance appearance;
        appearance.base = BaseType::Sky;
        appearance.retouches = {{3, {255, 255, 255}}};
        appearance.level = 500;
        const auto scene = renderAppearance(appearance, sky, 0.0, 12.0).value_or(LiveScene{});
        QCOMPARE(colourOf(scene, 1), (Bytes{13, 35, 85}));      // the sky of noon at half level
        QCOMPARE(colourOf(scene, 3), (Bytes{128, 128, 128}));   // the retouch over the sky, at half level
    }

    void aBlackBaseWithRetouchesIsOneColourPerKey()
    {
        Appearance appearance; // black base
        appearance.retouches = {{1, {255, 255, 255}}, {3, {0, 0, 255}}};
        const LiveScene scene = render(appearance);
        QCOMPARE(colourOf(scene, 0), (Bytes{0, 0, 0}));
        QCOMPARE(colourOf(scene, 1), (Bytes{255, 255, 255}));
        QCOMPARE(colourOf(scene, 3), (Bytes{0, 0, 255}));
        QCOMPARE(colourOf(scene, 4), (Bytes{0, 0, 0}));
    }

    void aRetouchLightsAnLedEvenWhereThereIsNoPositionForIt()
    {
        Appearance appearance;
        appearance.retouches = {{9, {1, 2, 3}}};
        QCOMPARE(colourOf(render(appearance), 9), (Bytes{1, 2, 3}));
    }

    void theRainbowGoesFromTheLeftEndToTheRightEnd()
    {
        Appearance appearance;
        appearance.base = BaseType::Rainbow;
        const LiveScene scene = render(appearance);
        QCOMPARE(colourOf(scene, 0), (Bytes{244, 37, 37}));   // place 0: red
        QCOMPARE(colourOf(scene, 2), (Bytes{37, 244, 151}));  // the middle: a green-cyan
        QCOMPARE(colourOf(scene, 4), (Bytes{244, 37, 223}));  // place 1: magenta, not red again (85 % of the circle)
    }

    void aFixedRainbowIgnoresTheTime()
    {
        Appearance appearance;
        appearance.base = BaseType::Rainbow;
        QVERIFY(render(appearance, 0.0).colours == render(appearance, 123.4).colours);
    }

    void aMovingRainbowTurnsWithTheTimeAndComesBackAfterAFullTurn()
    {
        Appearance appearance;
        appearance.base = BaseType::Rainbow;
        appearance.rainbowSpeed = 1000; // one turn a second
        QCOMPARE(colourOf(render(appearance, 0.25), 0), (Bytes{140, 244, 37}));   // a quarter of a turn: hue 0.25
        QVERIFY(render(appearance, 0.25).colours != render(appearance, 0.0).colours);
        QVERIFY(render(appearance, 1.0).colours == render(appearance, 0.0).colours); // a full turn
        QCOMPARE(colourOf(render(appearance, 0.5), 4), (Bytes{37, 244, 58}));      // place 1 after half a turn: hue wraps past 1
    }

    void aSlowerRainbowTurnsSlower()
    {
        Appearance slow;
        slow.base = BaseType::Rainbow;
        slow.rainbowSpeed = 100; // a tenth of a turn a second: 2.5 s is a quarter of a turn
        QVERIFY(render(slow, 2.5).colours == render([] { Appearance a; a.base = BaseType::Rainbow; a.rainbowSpeed = 1000; return a; }(), 0.25).colours);
    }

    void retouchesStayWhereTheyAreWhileTheRainbowMoves()
    {
        Appearance appearance;
        appearance.base = BaseType::Rainbow;
        appearance.rainbowSpeed = 500;
        appearance.retouches = {{2, {255, 255, 255}}};
        for (const double seconds : {0.0, 0.7, 1.9, 5.5})
            QCOMPARE(colourOf(render(appearance, seconds), 2), (Bytes{255, 255, 255}));
    }

    void theLevelIsAFactorOnEveryColourRetouchesIncluded()
    {
        Appearance appearance;
        appearance.colour = {200, 100, 50};
        appearance.retouches = {{1, {255, 255, 255}}};
        appearance.level = 500;
        const LiveScene half = render(appearance);
        QCOMPARE(colourOf(half, 0), (Bytes{100, 50, 25}));
        QCOMPARE(colourOf(half, 1), (Bytes{128, 128, 128})); // 127.5 rounds to 128
        appearance.level = 0;
        QCOMPARE(colourOf(render(appearance), 0), (Bytes{0, 0, 0}));
        QCOMPARE(colourOf(render(appearance), 1), (Bytes{0, 0, 0}));
        appearance.level = LevelFull;
        QCOMPARE(colourOf(render(appearance), 0), (Bytes{200, 100, 50})); // full level leaves the colours as they are
    }

    void anAppearanceOutOfRangeMakesNoScene()
    {
        const auto refused = [](const Appearance &appearance) { return !renderAppearance(appearance, fivePositions(), 0.0).has_value() && !isValid(appearance); };
        Appearance a;
        a.level = 1001;
        QVERIFY(refused(a));
        a = {};
        a.level = -1;
        QVERIFY(refused(a));
        a = {};
        a.rainbowSpeed = 1001;
        QVERIFY(refused(a));
        a = {};
        a.rainbowSpeed = -1;
        QVERIFY(refused(a));
        a = {};
        a.retouches = {{146, {1, 1, 1}}};
        QVERIFY(refused(a));
        a = {};
        a.retouches = {{5, {1, 1, 1}}, {5, {2, 2, 2}}}; // the same LED twice
        QVERIFY(refused(a));
        a = {};
        for (std::uint16_t led = 0; led < LiveLedCount; ++led)
            a.retouches.push_back({led, {1, 1, 1}});
        QVERIFY(isValid(a));                                // 146 retouches are the most
        a.retouches.push_back({0, {1, 1, 1}});
        QVERIFY(refused(a));
    }

    void noPositionAtAllOrAPositionBeyondTheFrameMakesNoScene()
    {
        const Appearance appearance;
        QVERIFY(!renderAppearance(appearance, {}, 0.0).has_value());
        const std::vector<LedPosition> wild{{146, 1, 1}};
        QVERIFY(!renderAppearance(appearance, wild, 0.0).has_value());
    }

    void theRainbowColourIsTheSameForTheKeyboardAndForThePreview()
    {
        // The one function that both use: a known place, and a turn that goes past one or below zero.
        const Rgb red = rainbowColour(0.0, 0.0);
        QCOMPARE((Bytes{red.red, red.green, red.blue}), (Bytes{244, 37, 37}));
        QVERIFY(rainbowColour(0.3, 1.0) == rainbowColour(0.3, 0.0));                                // a whole turn changes nothing
        QVERIFY(rainbowColour(0.3, -0.25) == rainbowColour(0.3, 0.75));                             // and a negative turn is the same as the rest of it
    }
};

QTEST_APPLESS_MAIN(AppearanceTest)
#include "test_appearance.moc"
