// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

// WHEN the keys are watched for the reactive glow: only while the person chose it AND the live lighting is on, and what the source says goes to the lighting
// at once as a LED and is kept nowhere. The source and the lighting are replaced by fakes, so that the rule of the driver is what runs.
#include "KeyGlowDriver.h"

#include <QSignalSpy>
#include <QTest>

#include <atomic>
#include <vector>

using namespace galleond;

namespace {

class FakeSource final : public KeyPressSource
{
    Q_OBJECT

public:
    void start() override
    {
        ++starts;
        if (canRead)
            m_watching = true;
        else
            Q_EMIT unavailable();
    }
    void stop() override
    {
        ++stops;
        m_watching = false;
    }
    [[nodiscard]] bool watching() const override { return m_watching; }
    void say(quint16 usage) { Q_EMIT keyDown(usage); }
    void goAway()
    {
        m_watching = false;
        Q_EMIT unavailable();
    }

    int starts = 0;
    int stops = 0;
    bool canRead = true;

private:
    bool m_watching = false;
};

class FakeTarget final : public KeyGlowTarget
{
public:
    [[nodiscard]] bool lightingOn() const override { return on; }
    void setGlow(bool value) override
    {
        glow = value;
        glowCalls.push_back(value);
    }
    void touch(std::uint16_t led) override { touches.push_back(led); }
    [[nodiscard]] std::optional<std::uint16_t> ledForUsage(galleon::keyboard::HidUsage usage) const override
    {
        if (usage == 0x3B)
            return std::uint16_t{59}; // F2
        return std::nullopt;           // a key whose light was not measured
    }

    bool on = false;
    bool glow = false;
    std::vector<bool> glowCalls;
    std::vector<std::uint16_t> touches;
};

} // namespace

class TestKeyGlowDriver : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void theKeysAreWatchedOnlyWhenTheEffectIsChosenAndTheLightingIsOn_data()
    {
        QTest::addColumn<bool>("chosen");
        QTest::addColumn<bool>("lightingOn");
        QTest::addColumn<bool>("watch");
        QTest::newRow("both") << true << true << true;
        QTest::newRow("chosen, lighting off") << true << false << false;
        QTest::newRow("not chosen, lighting on") << false << true << false;
        QTest::newRow("neither") << false << false << false;
    }
    void theKeysAreWatchedOnlyWhenTheEffectIsChosenAndTheLightingIsOn()
    {
        QFETCH(const bool, chosen);
        QFETCH(const bool, lightingOn);
        QFETCH(const bool, watch);
        QCOMPARE(KeyGlowDriver::shouldWatch(chosen, lightingOn), watch);
        FakeSource source;
        FakeTarget target;
        target.on = lightingOn;
        KeyGlowDriver driver(source, target, [chosen] { return chosen; });
        driver.refresh();
        QCOMPARE(source.watching(), watch);
        QCOMPARE(target.glow, watch);
        QCOMPARE(source.starts, watch ? 1 : 0);
    }

    void nothingIsReadBeforeTheFirstLookAndNothingStartsItButTheDriver()
    {
        FakeSource source;
        FakeTarget target;
        target.on = true;
        const KeyGlowDriver driver(source, target, [] { return true; });
        QCOMPARE(source.starts, 0); // constructing the driver does not start the source: only a look at the choice does
        QVERIFY(!source.watching());
    }

    void theChoiceIsReadAgainEverySecondSoUntickingTheBoxStopsTheWatchWithinTheSecond()
    {
        FakeSource source;
        FakeTarget target;
        target.on = true;
        std::atomic<bool> chosen{true};
        KeyGlowDriver driver(source, target, [&chosen] { return chosen.load(); });
        driver.setTimes(40, 5000, 2000);
        QTRY_VERIFY_WITH_TIMEOUT(source.watching(), 2000);
        chosen = false;
        QTRY_VERIFY_WITH_TIMEOUT(!source.watching(), 2000);
        QVERIFY(!target.glow);
        QCOMPARE(source.starts, 1);
    }

    void theLightingGoingOffStopsTheWatchAndTheGlowAndItComesBackWhenTheLightingDoes()
    {
        FakeSource source;
        FakeTarget target;
        target.on = true;
        KeyGlowDriver driver(source, target, [] { return true; });
        driver.refresh();
        QVERIFY(source.watching());
        target.on = false;
        driver.refresh();
        QVERIFY(!source.watching());
        QVERIFY(!target.glow);
        target.on = true;
        driver.refresh();
        QVERIFY(source.watching());
        QVERIFY(target.glow);
        QCOMPARE(source.starts, 2);
    }

    void aKeyThatWentDownReachesTheLightingAsItsLedAndNothingIsKeptHere()
    {
        FakeSource source;
        FakeTarget target;
        target.on = true;
        KeyGlowDriver driver(source, target, [] { return true; });
        driver.refresh();
        source.say(0x3B);
        source.say(0x3B);
        source.say(0x04); // a key whose light was not measured: no LED, no glow
        QCOMPARE(target.touches, (std::vector<std::uint16_t>{59, 59}));
    }

    void aKeySaidWhileTheWatchIsOverIsNotGivenToTheLighting()
    {
        FakeSource source;
        FakeTarget target;
        target.on = false;
        KeyGlowDriver driver(source, target, [] { return true; });
        driver.refresh();
        source.say(0x3B); // the source is not watching: nothing is relied on from it, but if it said something anyway the glow is off in the lighting
        QVERIFY(!target.glow);
    }

    void aSourceThatCannotReadIsTriedAgainLaterAndNotInALoop()
    {
        FakeSource source;
        source.canRead = false;
        FakeTarget target;
        target.on = true;
        KeyGlowDriver driver(source, target, [] { return true; });
        driver.setTimes(20, 400, 100);
        QTRY_COMPARE_WITH_TIMEOUT(source.starts, 1, 1000);
        QTest::qWait(250);
        QCOMPARE(source.starts, 1); // not tried again before the delay
        QVERIFY(!target.glow);
        QTRY_COMPARE_WITH_TIMEOUT(source.starts, 2, 2000); // and again after it
        source.canRead = true;
        QTRY_VERIFY_WITH_TIMEOUT(source.watching(), 2000);
        QVERIFY(target.glow);
    }

    void whenTheKeyboardGoesAwayTheGlowGoesWithItAndTheWatchComesBackWhenItDoes()
    {
        FakeSource source;
        FakeTarget target;
        target.on = true;
        KeyGlowDriver driver(source, target, [] { return true; });
        driver.setTimes(20, 200, 100);
        QTRY_VERIFY_WITH_TIMEOUT(source.watching(), 1000);
        source.goAway();
        QVERIFY(!target.glow);
        QTRY_VERIFY_WITH_TIMEOUT(source.watching(), 2000);
        QVERIFY(target.glow);
    }

    void theTraceIsToldBeforeTheFirstKeyCanBeReadAndOnlyAfterTheLastGlowingFrameIsGone()
    {
        FakeSource source;
        FakeTarget target;
        target.on = true;
        std::atomic<bool> chosen{true};
        KeyGlowDriver driver(source, target, [&chosen] { return chosen.load(); });
        std::vector<std::pair<bool, bool>> told; // what was said, and whether the source was already watching
        driver.setWatchingHook([&](bool watching) { told.emplace_back(watching, source.watching()); });
        driver.setTimes(20, 5000, 200);
        QTRY_VERIFY_WITH_TIMEOUT(source.watching(), 1000);
        QCOMPARE(told.size(), std::size_t{1});
        QVERIFY(told.front().first);
        QVERIFY(!told.front().second); // said before the source started
        chosen = false;
        QTRY_VERIFY_WITH_TIMEOUT(!source.watching(), 1000);
        QCOMPARE(told.size(), std::size_t{1}); // not told at once that it stopped: a frame that glows may still go
        QTRY_COMPARE_WITH_TIMEOUT(told.size(), std::size_t{2}, 2000);
        QVERIFY(!told.back().first);
    }

    void startingAgainBeforeTheTraceResumedKeepsItPaused()
    {
        FakeSource source;
        FakeTarget target;
        target.on = true;
        KeyGlowDriver driver(source, target, [] { return true; });
        std::vector<bool> told;
        driver.setWatchingHook([&](bool watching) { told.push_back(watching); });
        driver.setTimes(10000, 5000, 300);
        driver.refresh();
        target.on = false;
        driver.refresh(); // stopped
        target.on = true;
        driver.refresh(); // started again at once
        QTest::qWait(500);
        QCOMPARE(told, (std::vector<bool>{true})); // never told that it resumed
    }

    void destroyingTheDriverStopsTheWatchAndTheGlowAndResumesTheTrace()
    {
        FakeSource source;
        FakeTarget target;
        target.on = true;
        std::vector<bool> told;
        {
            KeyGlowDriver driver(source, target, [] { return true; });
            driver.setWatchingHook([&](bool watching) { told.push_back(watching); });
            driver.refresh();
            QVERIFY(source.watching());
        }
        QVERIFY(!source.watching());
        QVERIFY(!target.glow);
        QCOMPARE(told, (std::vector<bool>{true, false}));
    }
};

QTEST_GUILESS_MAIN(TestKeyGlowDriver)
#include "test_keyglowdriver.moc"
