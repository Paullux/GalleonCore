// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

// The positions of the LEDs compiled into galleond (data/devices/galleon-100-sd-led-positions.json): they are read, they agree with the key map, and a
// damaged file gives nothing.

#include "KeyMapFile.h"
#include "LedPositions.h"

#include <galleon/keyboard/Appearance.h>

#include <QTest>

#include <algorithm>
#include <set>

using namespace galleond;
using galleon::keyboard::LedPosition;

namespace {

QByteArray file(const char *leds)
{
    return QByteArray(R"({"version":1,"unit":"mm","leds":)") + leds + "}";
}

const LedPosition *find(const std::vector<LedPosition> &all, std::uint16_t led)
{
    const auto it = std::ranges::find_if(all, [led](const LedPosition &p) { return p.led == led; });
    return it == all.end() ? nullptr : &*it;
}

} // namespace

class TestLedPositions : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void theBuiltInPositionsAreReadAndCoverTheKeysAndTheStrip()
    {
        const auto &positions = builtInLedPositions();
        QCOMPARE(positions.size(), std::size_t{95}); // 88 keys and the 7 sections of the light strip
        std::set<int> leds;
        for (const LedPosition &p : positions)
            leds.insert(p.led);
        QCOMPARE(leds.size(), std::size_t{95}); // none twice
        QVERIFY(*leds.rbegin() <= 145);
    }

    void everyLedThatLightsSomethingInTheKeyMapHasAPlaceAndTheOthersHaveNone()
    {
        const auto &positions = builtInLedPositions();
        const QString path = QStringLiteral(QT_TESTCASE_SOURCEDIR) + QStringLiteral("/../data/devices/galleon-100-sd-keymap.json");
        const auto map = readKeyMap(path);
        QVERIFY(map.has_value());
        const auto &keyMap = map.value_or(KeyMapFile{}).map;
        for (std::uint16_t led = 0; led < galleon::keyboard::LiveLedCount; ++led) {
            const auto answer = keyMap.assignmentFor(led);
            QVERIFY(answer.has_value());
            const bool lights = answer.value_or(galleon::keyboard::LedAssignment{}).kind != galleon::keyboard::LedKind::Unlit;
            QCOMPARE(find(positions, led) != nullptr, lights);
        }
    }

    void thePlacesAreWhereTheDrawingPutsThem()
    {
        const auto &positions = builtInLedPositions();
        const LedPosition *q = find(positions, 4);    // the key engraved Q
        const LedPosition *first = find(positions, 139);
        const LedPosition *last = find(positions, 145);
        QVERIFY(q != nullptr && first != nullptr && last != nullptr);
        QVERIFY(first->x < last->x);                  // the strip runs from left to right
        QVERIFY(first->y < q->y);                     // and sits above the keys
    }

    void aDamagedOrOddFileGivesNothing()
    {
        const auto refused = [](const QByteArray &bytes) { return !parseLedPositions(bytes).has_value(); };
        QVERIFY(refused(""));
        QVERIFY(refused("not json"));
        QVERIFY(refused("[]"));
        QVERIFY(refused(R"({"version":2,"unit":"mm","leds":[{"led":1,"kind":"key","x":1,"y":1}]})"));
        QVERIFY(refused(R"({"version":1,"unit":"cm","leds":[{"led":1,"kind":"key","x":1,"y":1}]})"));
        QVERIFY(refused(file("[]")));                                                         // no LED
        QVERIFY(refused(file("5")));
        QVERIFY(refused(file(R"([5])")));
        QVERIFY(refused(file(R"([{"led":146,"kind":"key","x":1,"y":1}])")));                  // beyond the frame
        QVERIFY(refused(file(R"([{"led":-1,"kind":"key","x":1,"y":1}])")));
        QVERIFY(refused(file(R"([{"led":1.5,"kind":"key","x":1,"y":1}])")));
        QVERIFY(refused(file(R"([{"led":1,"kind":"lamp","x":1,"y":1}])")));                   // a kind that is not one
        QVERIFY(refused(file(R"([{"led":1,"kind":"key","x":"1","y":1}])")));
        QVERIFY(refused(file(R"([{"led":1,"kind":"key","y":1}])")));                          // no x
        QVERIFY(refused(file(R"([{"led":1,"kind":"key","x":1e9,"y":1}])")));                  // not a place on a keyboard
        QVERIFY(refused(file(R"([{"led":1,"kind":"key","x":1,"y":1},{"led":1,"kind":"key","x":2,"y":2}])"))); // a LED twice
        QVERIFY(refused(QByteArray(70000, ' ')));
        QVERIFY(parseLedPositions(file(R"([{"led":1,"kind":"key","x":1,"y":1},{"led":145,"kind":"strip","x":-5.5,"y":999}])")).has_value());
    }

    void aRefusalNeverQuotesTheFile()
    {
        QString why;
        QVERIFY(!parseLedPositions(R"({"version":1,"unit":"SECRET-TEXT","leds":[]})", &why).has_value());
        QVERIFY(!why.isEmpty());
        QVERIFY(!why.contains(QStringLiteral("SECRET-TEXT")));
    }

    void theRainbowOnTheRealPositionsRunsFromRedAtTheLeftEndToMagentaAtTheRightEnd()
    {
        const auto &positions = builtInLedPositions();
        galleon::keyboard::Appearance appearance;
        appearance.base = galleon::keyboard::BaseType::Rainbow;
        const auto scene = galleon::keyboard::renderAppearance(appearance, positions, 0.0);
        QVERIFY(scene.has_value());
        const auto &s = scene.value_or(galleon::keyboard::LiveScene{});
        const auto leftmost = std::ranges::min_element(positions, {}, &LedPosition::x);
        const auto rightmost = std::ranges::max_element(positions, {}, &LedPosition::x);
        const auto colourAt = [&s](std::uint16_t led) {
            const std::size_t at = 3 * static_cast<std::size_t>(led);
            return std::array<std::uint8_t, 3>{s.colours.at(at), s.colours.at(at + 1), s.colours.at(at + 2)};
        };
        QCOMPARE(colourAt(leftmost->led), (std::array<std::uint8_t, 3>{244, 37, 37}));
        QCOMPARE(colourAt(rightmost->led), (std::array<std::uint8_t, 3>{244, 37, 223}));
        QCOMPARE(colourAt(0), (std::array<std::uint8_t, 3>{0, 0, 0})); // LED 0 lights nothing (the calibration saw it): dark
    }
};

QTEST_GUILESS_MAIN(TestLedPositions)
#include "test_ledpositions.moc"
