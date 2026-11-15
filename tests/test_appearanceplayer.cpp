// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: LGPL-3.0-or-later

// Playing an appearance over time: a scene is given only when it is new. Pure: the time is an argument, nothing is sent.

#include <galleon/keyboard/AppearancePlayer.h>

#include <QTest>

#include <vector>

using namespace galleon::keyboard;

namespace {

std::vector<LedPosition> fivePositions()
{
    return {{0, 0, 10}, {1, 10, 10}, {2, 20, 10}, {3, 30, 10}, {4, 40, 10}};
}

Appearance rainbow(int speed)
{
    Appearance a;
    a.base = BaseType::Rainbow;
    a.rainbowSpeed = speed;
    return a;
}

AppearancePlayer ready(const Appearance &appearance, double at = 0.0)
{
    AppearancePlayer player;
    player.setPositions(fivePositions());
    static_cast<void>(player.setAppearance(appearance, at)); // a refusal would show at once: the tests ask for a scene
    return player;
}

} // namespace

class AppearancePlayerTest : public QObject
{
    Q_OBJECT

private slots:
    void aStillAppearanceGivesOneSceneAndThenNothing()
    {
        Appearance still;
        still.colour = {10, 20, 30};
        AppearancePlayer player = ready(still);
        QVERIFY(player.sceneAt(0.0).has_value());
        QVERIFY(!player.sceneAt(0.05).has_value());
        QVERIFY(!player.sceneAt(10.0).has_value());
    }

    void aFixedRainbowGivesOneSceneToo()
    {
        AppearancePlayer player = ready(rainbow(0));
        QVERIFY(player.sceneAt(0.0).has_value());
        for (const double seconds : {0.05, 1.0, 100.0})
            QVERIFY(!player.sceneAt(seconds).has_value());
    }

    void aFastRainbowGivesANewSceneAtEveryStep()
    {
        AppearancePlayer player = ready(rainbow(1000)); // one turn a second
        int given = 0;
        for (int step = 0; step < 20; ++step) // 20 steps of 50 ms: a whole turn
            given += player.sceneAt(step * 0.05).has_value() ? 1 : 0;
        QCOMPARE(given, 20);
    }

    void aSlowRainbowGivesAScenePerRealChangeOfColourAndNotOnePerStep()
    {
        // One LED, so the count is that of its own colour: at one thousandth of a turn a second a colour channel moves by one step only every
        // sixteen or so steps of 50 ms.
        AppearancePlayer player;
        player.setPositions({{0, 0, 10}});
        QVERIFY(player.setAppearance(rainbow(1), 0.0));
        int given = 0;
        for (int step = 0; step < 400; ++step) // 20 s
            given += player.sceneAt(step * 0.05).has_value() ? 1 : 0;
        QVERIFY2(given >= 10, "the rainbow does move");
        QVERIFY2(given <= 60, "far fewer scenes than steps: only the real changes are given");
    }

    void aChangeOfAppearanceGivesANewSceneEvenIfItLooksTheSame()
    {
        Appearance still;
        still.colour = {5, 5, 5};
        AppearancePlayer player = ready(still);
        QVERIFY(player.sceneAt(0.0).has_value());
        QVERIFY(!player.sceneAt(1.0).has_value());
        QVERIFY(player.setAppearance(still, 2.0)); // asked for again: it is shown again
        QVERIFY(player.sceneAt(2.0).has_value());
        QVERIFY(!player.sceneAt(3.0).has_value());
    }

    void resendGivesTheSameSceneAgainOnce()
    {
        Appearance still;
        still.colour = {5, 5, 5};
        AppearancePlayer player = ready(still);
        const auto first = player.sceneAt(0.0);
        QVERIFY(first.has_value());
        QVERIFY(!player.sceneAt(1.0).has_value());
        player.resend(); // the keys were taken back
        const auto again = player.sceneAt(2.0);
        QVERIFY(again.has_value());
        QVERIFY(again == first);
        QVERIFY(!player.sceneAt(3.0).has_value());
    }

    void aMovingRainbowStartsAtTheBeginningOfItsTurnWhenItIsSet()
    {
        AppearancePlayer player = ready(rainbow(1000), 100.25); // set a quarter of a turn after a whole number of turns
        const auto scene = player.sceneAt(100.25);
        const auto expected = renderAppearance(rainbow(1000), fivePositions(), 0.0);
        QVERIFY(scene.has_value());
        QVERIFY(scene == expected);
    }

    void retouchesStayWhileTheRainbowMoves()
    {
        Appearance appearance = rainbow(1000);
        appearance.retouches = {{2, {255, 255, 255}}};
        AppearancePlayer player = ready(appearance);
        for (int step = 0; step < 5; ++step) {
            const auto scene = player.sceneAt(step * 0.05);
            QVERIFY(scene.has_value());
            QCOMPARE(scene.value_or(LiveScene{}).colours.at(6), std::uint8_t{255}); // LED 2, red
            QCOMPARE(scene.value_or(LiveScene{}).colours.at(7), std::uint8_t{255});
            QCOMPARE(scene.value_or(LiveScene{}).colours.at(8), std::uint8_t{255});
        }
    }

    void anAppearanceThatIsNotValidIsRefusedAndTheOneBeforeKeepsPlaying()
    {
        Appearance still;
        still.colour = {9, 9, 9};
        AppearancePlayer player = ready(still);
        Appearance wild;
        wild.level = 5000;
        QVERIFY(!player.setAppearance(wild, 1.0));
        const auto scene = player.sceneAt(1.0);
        QVERIFY(scene.has_value());
        QCOMPARE(scene.value_or(LiveScene{}).colours.at(0), std::uint8_t{9});
    }

    void withoutPositionsOrWithoutAnAppearanceNothingIsGiven()
    {
        AppearancePlayer player;
        QVERIFY(!player.ready());
        QVERIFY(!player.sceneAt(0.0).has_value());
        QVERIFY(player.setAppearance(Appearance{}, 0.0));
        QVERIFY(!player.sceneAt(0.0).has_value()); // still no position
        player.setPositions(fivePositions());
        QVERIFY(player.ready());
        QVERIFY(player.sceneAt(0.0).has_value());
    }
};

QTEST_APPLESS_MAIN(AppearancePlayerTest)
#include "test_appearanceplayer.moc"
