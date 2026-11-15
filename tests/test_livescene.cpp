// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: LGPL-3.0-or-later

// What the live lighting shows (one colour for every key, or one lit LED) and the frame that carries it. Pure: nothing is sent.

#include <galleon/keyboard/LiveScene.h>

#include <QTest>

#include <array>
#include <vector>

using namespace galleon::keyboard;

namespace {

std::array<std::uint8_t, 3> ledOf(const LiveFrame &frame, std::size_t led)
{
    const std::size_t at = 2 + 3 * led;
    return {frame.at(at), frame.at(at + 1), frame.at(at + 2)};
}

} // namespace

class LiveSceneTest : public QObject
{
    Q_OBJECT

private slots:
    void aUniformSceneLightsEveryLedWithOneColour()
    {
        const auto scene = uniformScene(10, 20, 30);
        QVERIFY(scene.has_value());
        const auto frame = sceneFrame(scene.value_or(LiveScene{}));
        QVERIFY(frame.has_value());
        const LiveFrame &f = frame.value_or(LiveFrame{});
        QCOMPARE(f.at(0), std::uint8_t{0x12}); // the header of the frame, `12 00`
        QCOMPARE(f.at(1), std::uint8_t{0x00});
        for (const std::size_t led : {std::size_t{0}, std::size_t{0x3A}, LiveLedCount - 1})
            QCOMPARE(ledOf(f, led), (std::array<std::uint8_t, 3>{10, 20, 30}));
    }

    void aSingleLedSceneLightsThatLedAndNothingElse()
    {
        const auto scene = singleLedScene(0x3A, 0, 255, 0); // F1, green
        QVERIFY(scene.has_value());
        const auto frame = sceneFrame(scene.value_or(LiveScene{}));
        QVERIFY(frame.has_value());
        const LiveFrame &f = frame.value_or(LiveFrame{});
        QCOMPARE(f.at(0), std::uint8_t{0x12});
        int lit = 0;
        for (std::size_t led = 0; led < LiveLedCount; ++led) {
            if (ledOf(f, led) != std::array<std::uint8_t, 3>{0, 0, 0})
                ++lit;
        }
        QCOMPARE(lit, 1);
        QCOMPARE(ledOf(f, 0x3A), (std::array<std::uint8_t, 3>{0, 255, 0}));
    }

    void theFirstAndTheLastLedCanBeLitAlone()
    {
        const auto first = sceneFrame(singleLedScene(0, 1, 2, 3).value_or(LiveScene{}));
        const auto last = sceneFrame(singleLedScene(145, 4, 5, 6).value_or(LiveScene{}));
        QVERIFY(first.has_value() && last.has_value());
        QCOMPARE(ledOf(first.value_or(LiveFrame{}), 0), (std::array<std::uint8_t, 3>{1, 2, 3}));
        QCOMPARE(ledOf(last.value_or(LiveFrame{}), 145), (std::array<std::uint8_t, 3>{4, 5, 6}));
    }

    void aValueOutOfRangeMakesNoScene()
    {
        QVERIFY(!uniformScene(256, 0, 0));
        QVERIFY(!uniformScene(0, -1, 0));
        QVERIFY(!uniformScene(0, 0, 1000));
        QVERIFY(!singleLedScene(146, 1, 1, 1)); // beyond the 146 LEDs of the frame
        QVERIFY(!singleLedScene(-1, 1, 1, 1));
        QVERIFY(!singleLedScene(5, 256, 0, 0));
        QVERIFY(uniformScene(0, 0, 0).has_value()); // black is a colour
    }

    void aPerLedSceneLightsEachLedWithItsOwnColourAndTheOthersStayBlack()
    {
        const std::vector<LedColor> leds{{0x3A, 255, 0, 0}, {0x04, 0, 255, 0}, {145, 1, 2, 3}};
        const auto scene = perLedScene(leds);
        QVERIFY(scene.has_value());
        const auto frame = sceneFrame(scene.value_or(LiveScene{}));
        QVERIFY(frame.has_value());
        const LiveFrame &f = frame.value_or(LiveFrame{});
        QCOMPARE(f.at(0), std::uint8_t{0x12});
        QCOMPARE(ledOf(f, 0x3A), (std::array<std::uint8_t, 3>{255, 0, 0}));
        QCOMPARE(ledOf(f, 0x04), (std::array<std::uint8_t, 3>{0, 255, 0}));
        QCOMPARE(ledOf(f, 145), (std::array<std::uint8_t, 3>{1, 2, 3}));
        int lit = 0;
        for (std::size_t led = 0; led < LiveLedCount; ++led)
            lit += ledOf(f, led) != std::array<std::uint8_t, 3>{0, 0, 0} ? 1 : 0;
        QCOMPARE(lit, 3);
    }

    void aLaterEntryForTheSameLedReplacesTheEarlierOne()
    {
        const std::vector<LedColor> leds{{7, 1, 1, 1}, {7, 9, 9, 9}};
        const auto frame = sceneFrame(perLedScene(leds).value_or(LiveScene{}));
        QVERIFY(frame.has_value());
        QCOMPARE(ledOf(frame.value_or(LiveFrame{}), 7), (std::array<std::uint8_t, 3>{9, 9, 9}));
    }

    void aPerLedSceneWithANonsensicalListMakesNoScene()
    {
        QVERIFY(!perLedScene({}).has_value());                                   // nothing to show
        QVERIFY(!perLedScene(std::vector<LedColor>{{146, 1, 1, 1}}).has_value()); // beyond the 146 LEDs of the frame
        QVERIFY(!perLedScene(std::vector<LedColor>{{65535, 1, 1, 1}}).has_value());
        const std::vector<LedColor> tooMany(LiveLedCount + 1, LedColor{3, 1, 1, 1});
        QVERIFY(!perLedScene(tooMany).has_value());                              // more entries than LEDs
        const std::vector<LedColor> everyLed = [] {
            std::vector<LedColor> all;
            for (std::size_t led = 0; led < LiveLedCount; ++led)
                all.push_back({static_cast<std::uint16_t>(led), 1, 2, 3});
            return all;
        }();
        QVERIFY(perLedScene(everyLed).has_value());                              // 146 entries is the most
    }

    void aSceneMadeByHandWithAWildLedSendsNothing()
    {
        LiveScene wild;
        wild.kind = LiveScene::Kind::SingleLed;
        wild.led = 500;
        wild.red = 255;
        QVERIFY(!sceneFrame(wild).has_value()); // no frame, so no write, and never a frame without its header
    }
};

QTEST_APPLESS_MAIN(LiveSceneTest)
#include "test_livescene.moc"
