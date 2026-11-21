// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#include "FakeTransport.h"

#include <MatrixRainController.h>
#include <MatrixTrigger.h>
#include <ScreenRenderer.h>

#include <galleon/StreamDeck.h>

#include <QGuiApplication>
#include <QImage>
#include <QSignalSpy>
#include <QTest>

#include <algorithm>

#include <array>
#include <memory>

using namespace galleond;
using Action = MatrixTrigger::Action;
using Kind = MatrixTrigger::Kind;
using galleon::StreamDeck;

namespace {

const QString &widgetFile()
{
    static const QString file = QStringLiteral(GALLEON_SOURCE_WIDGETS "/MatrixRain.qml");
    return file;
}

// Dial button report (galleon-deck / node-elgato-stream-deck layout, see Protocol.h): states of both.
FakeTransport::Report dialButtons(bool left, bool right)
{
    return {0x01, 0x03, 0x03, 0x00, 0x00, static_cast<std::uint8_t>(left), static_cast<std::uint8_t>(right)};
}

struct GreenCount
{
    int green = 0;
    int other = 0; // clearly not black, not green
};

GreenCount classify(const QImage &image)
{
    GreenCount count;
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            const QColor c = image.pixelColor(x, y);
            if (c.green() > 60 && c.green() > c.red() + 20 && c.green() > c.blue() + 20)
                ++count.green;
            else if (c.red() > 80 || c.blue() > 80)
                ++count.other;
        }
    }
    return count;
}

struct Fixture
{
    FakeTransport *transport = nullptr;
    std::unique_ptr<StreamDeck> deck;
    std::unique_ptr<ScreenRenderer> renderer;
    RingSettings rings; // where the rings are set: the controller asks at each picture
    std::unique_ptr<MatrixRainController> controller;

    Fixture()
    {
        auto fake = std::make_unique<FakeTransport>();
        transport = fake.get();
        deck = std::make_unique<StreamDeck>(std::move(fake));
        QVERIFY(deck->open());
        renderer = std::make_unique<ScreenRenderer>(widgetFile());
        QVERIFY(renderer->isValid());
        controller = std::make_unique<MatrixRainController>(deck.get(), renderer.get());
        controller->setHoldMs(300);
        controller->setRingSettingsSource([this] { return rings; });
    }
};

} // namespace

class TestMatrixRain : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    // ---- the decision (pure) ----

    void bothDialsHeldForFiveSecondsStart()
    {
        MatrixTrigger trigger;
        QCOMPARE(trigger.onEvent(Kind::DialPressed, 0, 1000), Action::None);
        QCOMPARE(trigger.onEvent(Kind::DialPressed, 1, 1200), Action::None);
        QCOMPARE(trigger.msUntilStart(1200), 5000);
        QCOMPARE(trigger.tick(6199), Action::None);
        QCOMPARE(trigger.tick(6200), Action::Start);
        QVERIFY(trigger.isRunning());
    }

    void oneDialAloneNeverStarts()
    {
        MatrixTrigger trigger;
        trigger.onEvent(Kind::DialPressed, 1, 0);
        QCOMPARE(trigger.msUntilStart(100), -1);
        QCOMPARE(trigger.tick(60000), Action::None);
    }

    void releasingBeforeFiveSecondsCancels()
    {
        MatrixTrigger trigger;
        trigger.onEvent(Kind::DialPressed, 0, 0);
        trigger.onEvent(Kind::DialPressed, 1, 0);
        trigger.onEvent(Kind::DialReleased, 0, 4999);
        QCOMPARE(trigger.msUntilStart(4999), -1);
        QCOMPARE(trigger.tick(9000), Action::None);
        QVERIFY(!trigger.isRunning());
    }

    void aNewHoldStartsFromItsOwnSecondPress()
    {
        MatrixTrigger trigger;
        trigger.onEvent(Kind::DialPressed, 0, 0);
        trigger.onEvent(Kind::DialPressed, 1, 0);
        trigger.onEvent(Kind::DialReleased, 1, 4000);
        trigger.onEvent(Kind::DialPressed, 1, 4500);
        QCOMPARE(trigger.tick(5000), Action::None); // the first hold does not count
        QCOMPARE(trigger.tick(9500), Action::Start);
    }

    void turningADialOrPressingAKeyWhileHoldingCancels()
    {
        for (const Kind other : {Kind::DialRotated, Kind::KeyPressed, Kind::KeyReleased}) {
            MatrixTrigger trigger;
            trigger.onEvent(Kind::DialPressed, 0, 0);
            trigger.onEvent(Kind::DialPressed, 1, 0);
            trigger.onEvent(other, 0, 2000);
            QCOMPARE(trigger.tick(9000), Action::None);
        }
    }

    void startsOnlyOncePerHold()
    {
        MatrixTrigger trigger;
        trigger.onEvent(Kind::DialPressed, 0, 0);
        trigger.onEvent(Kind::DialPressed, 1, 0);
        QCOMPARE(trigger.tick(5000), Action::Start);
        QCOMPARE(trigger.tick(5001), Action::None);
        QCOMPARE(trigger.tick(60000), Action::None);
    }

    void releasingTheStartingDialsDoesNotStopIt()
    {
        MatrixTrigger trigger;
        trigger.onEvent(Kind::DialPressed, 0, 0);
        trigger.onEvent(Kind::DialPressed, 1, 0);
        QCOMPARE(trigger.tick(5000), Action::Start);
        QCOMPARE(trigger.onEvent(Kind::DialReleased, 0, 5300), Action::None);
        QCOMPARE(trigger.onEvent(Kind::DialReleased, 1, 5400), Action::None);
        QVERIFY(trigger.isRunning());
    }

    void theStartingDialsAreHeldUntilBothAreReleased()
    {
        MatrixTrigger trigger;
        QVERIFY(!trigger.startingDialsHeld()); // nothing runs
        trigger.onEvent(Kind::DialPressed, 0, 0);
        trigger.onEvent(Kind::DialPressed, 1, 0);
        QVERIFY(!trigger.startingDialsHeld()); // not started yet
        trigger.tick(5000);
        QVERIFY(trigger.startingDialsHeld());
        trigger.onEvent(Kind::DialReleased, 0, 5100);
        QVERIFY(trigger.startingDialsHeld()); // one finger is still on
        trigger.onEvent(Kind::DialReleased, 1, 5200);
        QVERIFY(!trigger.startingDialsHeld());
        QVERIFY(trigger.isRunning()); // and it goes on
    }

    void anyInputAfterTheStartStopsIt()
    {
        const std::array<std::pair<Kind, int>, 4> inputs{{{Kind::KeyPressed, 3}, {Kind::KeyReleased, 3}, {Kind::DialRotated, 1},
                                                          {Kind::DialPressed, 0}}};
        for (const auto &[kind, index] : inputs) {
            MatrixTrigger trigger;
            trigger.onEvent(Kind::DialPressed, 0, 0);
            trigger.onEvent(Kind::DialPressed, 1, 0);
            trigger.tick(5000);
            QCOMPARE(trigger.onEvent(Kind::DialReleased, 0, 5100), Action::None);
            QCOMPARE(trigger.onEvent(Kind::DialReleased, 1, 5100), Action::None);
            QCOMPARE(trigger.onEvent(kind, index, 6000), Action::Stop);
            QVERIFY(!trigger.isRunning());
        }
    }

    void releaseOfAnotherDialThanTheHeldOnesIsNotIgnoredTwice()
    {
        // Held dials are forgiven once each: a later press then release is a real input.
        MatrixTrigger trigger;
        trigger.onEvent(Kind::DialPressed, 0, 0);
        trigger.onEvent(Kind::DialPressed, 1, 0);
        trigger.tick(5000);
        QCOMPARE(trigger.onEvent(Kind::DialReleased, 0, 5100), Action::None);
        QCOMPARE(trigger.onEvent(Kind::DialPressed, 0, 5200), Action::Stop);
    }

    void stoppingWhileFingersAreStillDownDoesNotRestart()
    {
        MatrixTrigger trigger;
        trigger.onEvent(Kind::DialPressed, 0, 0);
        trigger.onEvent(Kind::DialPressed, 1, 0);
        trigger.tick(5000);
        QCOMPARE(trigger.onEvent(Kind::KeyPressed, 0, 5100), Action::Stop);
        QCOMPARE(trigger.tick(20000), Action::None);
    }

    void outOfRangeIndexesAreDropped()
    {
        MatrixTrigger trigger;
        trigger.onEvent(Kind::DialPressed, 0, 0);
        trigger.onEvent(Kind::DialPressed, 1, 0);
        QCOMPARE(trigger.onEvent(Kind::KeyPressed, 12, 100), Action::None);
        QCOMPARE(trigger.onEvent(Kind::DialRotated, -1, 100), Action::None);
        QCOMPARE(trigger.onEvent(Kind::DialReleased, 2, 100), Action::None);
        QCOMPARE(trigger.tick(5000), Action::Start); // none of them cancelled the hold
    }

    void resetForgetsEverything()
    {
        MatrixTrigger trigger;
        trigger.onEvent(Kind::DialPressed, 0, 0);
        trigger.onEvent(Kind::DialPressed, 1, 0);
        trigger.reset();
        QCOMPARE(trigger.tick(9000), Action::None);
        QVERIFY(!trigger.isRunning());
    }

    // ---- the animation ----

    void rainIsGreenOnBlack()
    {
        ScreenRenderer renderer(widgetFile());
        QVERIFY(renderer.isValid());
        const QImage frame = renderer.render({{QStringLiteral("tick"), 40}, {QStringLiteral("cell"), 18}}, QSize(720, 384));
        QCOMPARE(frame.size(), QSize(720, 384));
        const GreenCount count = classify(frame);
        QVERIFY2(count.green > 500, qPrintable(QString::number(count.green)));
        QVERIFY2(count.other < count.green / 4, qPrintable(QString::number(count.other)));
    }

    void rainFallsAndDiffersBetweenSurfaces()
    {
        ScreenRenderer renderer(widgetFile());
        const QSize key(160, 160);
        const auto render = [&](int tick, int seed) {
            return renderer.render({{QStringLiteral("tick"), tick}, {QStringLiteral("seed"), seed}, {QStringLiteral("cell"), 20}}, key);
        };
        const QImage first = render(30, 1);
        QVERIFY(first != render(31, 1)); // it moves
        QVERIFY(first != render(30, 2)); // another key rains differently
        QCOMPARE(first, render(30, 1));  // and it is a pure function of tick and seed
    }

    void rainIsEmptyOfLettersLongBeforeAndStaysBounded()
    {
        // A huge frame number (hours of rain) draws the same kind of picture: no state grows.
        ScreenRenderer renderer(widgetFile());
        const QImage late = renderer.render({{QStringLiteral("tick"), 100000000}, {QStringLiteral("cell"), 18}}, QSize(720, 384));
        QVERIFY(classify(late).green > 500);
    }

    // ---- the controller, on a fake module ----

    void holdingBothDialsDrawsGreenOnTheModuleAndAnnouncesIt()
    {
        Fixture f;
        const QSignalSpy active(f.controller.get(), &MatrixRainController::activeChanged);
        f.transport->writes.clear();
        f.transport->pendingReads.push_back(dialButtons(true, true));
        QTRY_COMPARE_WITH_TIMEOUT(active.count(), 1, 3000);
        QVERIFY(active.at(0).at(0).toBool());
        QVERIFY(f.controller->isActive());
        QVERIFY(!f.transport->writes.empty()); // pictures went to the module
    }

    void releasingBeforeTheHoldIsOverStartsNothing()
    {
        Fixture f;
        const QSignalSpy active(f.controller.get(), &MatrixRainController::activeChanged);
        f.transport->pendingReads.push_back(dialButtons(true, true));
        QTest::qWait(100);
        f.transport->pendingReads.push_back(dialButtons(true, false));
        QTest::qWait(600);
        QCOMPARE(active.count(), 0);
        QVERIFY(!f.controller->isActive());
    }

    void keyPressStopsItAndGivesTheScreenBack()
    {
        Fixture f;
        const QSignalSpy active(f.controller.get(), &MatrixRainController::activeChanged);
        f.transport->pendingReads.push_back(dialButtons(true, true));
        QTRY_COMPARE_WITH_TIMEOUT(active.count(), 1, 3000);
        f.transport->pendingReads.push_back(dialButtons(false, false)); // fingers lifted: it goes on
        QTest::qWait(200);
        QVERIFY(f.controller->isActive());
        f.transport->pendingReads.push_back({0x01, 0x00, 0x0c, 0x00, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}); // key 0
        QTRY_COMPARE_WITH_TIMEOUT(active.count(), 2, 3000);
        QVERIFY(!active.at(1).at(0).toBool());
        QVERIFY(!f.controller->isActive());
    }

    void theComputerIsWatchedOnlyAfterTheFingersLeftTheDialsThatStartedTheRain()
    {
        // The dials of the module are input devices of the system too: their release is seen as an action, so nothing may watch the computer before it.
        Fixture f;
        const QSignalSpy active(f.controller.get(), &MatrixRainController::activeChanged);
        const QSignalSpy free(f.controller.get(), &MatrixRainController::handsFree);
        f.transport->pendingReads.push_back(dialButtons(true, true));
        QTRY_COMPARE_WITH_TIMEOUT(active.count(), 1, 3000);
        QTest::qWait(MatrixRainController::HandsFreeGraceMs + 300);
        QCOMPARE(free.count(), 0); // the fingers are still on the dials, however long
        f.transport->pendingReads.push_back(dialButtons(false, true)); // one finger leaves
        QTest::qWait(MatrixRainController::HandsFreeGraceMs + 300);
        QCOMPARE(free.count(), 0);
        f.transport->pendingReads.push_back(dialButtons(false, false)); // the other one
        QTest::qWait(MatrixRainController::HandsFreeGraceMs / 2);
        QCOMPARE(free.count(), 0); // a moment of grace, for what the system says of the release
        QTRY_COMPARE_WITH_TIMEOUT(free.count(), 1, 2000);
        QVERIFY(f.controller->isActive());
        QTest::qWait(MatrixRainController::HandsFreeGraceMs + 300);
        QCOMPARE(free.count(), 1); // once
    }

    void nothingIsSaidFreeWhenTheRainEndsBeforeTheFingersLeft()
    {
        Fixture f;
        const QSignalSpy active(f.controller.get(), &MatrixRainController::activeChanged);
        const QSignalSpy free(f.controller.get(), &MatrixRainController::handsFree);
        f.transport->pendingReads.push_back(dialButtons(true, true));
        QTRY_COMPARE_WITH_TIMEOUT(active.count(), 1, 3000);
        f.controller->endNow();
        QTRY_COMPARE_WITH_TIMEOUT(active.count(), 2, 3000);
        f.transport->pendingReads.push_back(dialButtons(false, false));
        QTest::qWait(MatrixRainController::HandsFreeGraceMs + 400);
        QCOMPARE(free.count(), 0);
    }

    void aSecondRainSaysItsOwnFreeAgain()
    {
        Fixture f;
        const QSignalSpy active(f.controller.get(), &MatrixRainController::activeChanged);
        const QSignalSpy free(f.controller.get(), &MatrixRainController::handsFree);
        for (int round = 1; round <= 2; ++round) {
            f.transport->pendingReads.push_back(dialButtons(true, true));
            QTRY_COMPARE_WITH_TIMEOUT(active.count(), round + round - 1, 3000);
            f.transport->pendingReads.push_back(dialButtons(false, false));
            QTRY_COMPARE_WITH_TIMEOUT(free.count(), round, 2000);
            f.controller->endNow();
            QTRY_COMPARE_WITH_TIMEOUT(active.count(), round + round, 3000);
        }
    }

    // The reports that light the segments of the rings: `03 24 <led> <red> <green> <blue>`.
    static std::vector<std::vector<std::uint8_t>> ringReports(const FakeTransport &transport)
    {
        std::vector<std::vector<std::uint8_t>> reports;
        for (const auto &report : transport.featureReports) {
            if (report.size() > 5 && report[0] == 0x03 && report[1] == 0x24)
                reports.push_back(report);
        }
        return reports;
    }

    void theRingsOfTheDialsShowTheRainToo()
    {
        const Fixture f;
        const QSignalSpy active(f.controller.get(), &MatrixRainController::activeChanged);
        f.transport->featureReports.clear();
        f.transport->pendingReads.push_back(dialButtons(true, true));
        QTRY_COMPARE_WITH_TIMEOUT(active.count(), 1, 3000);
        QTRY_VERIFY_WITH_TIMEOUT(ringReports(*f.transport).size() >= 8, 3000); // every segment of both rings is given at the first picture
        bool leftHead = false;
        std::array<bool, 8> ledSeen{};
        for (const auto &report : ringReports(*f.transport)) {
            QVERIFY(report[2] < 8); // the four segments of the two rings, and no other LED
            ledSeen.at(report[2]) = true;
            leftHead = leftHead || (report[2] >= 4 && report[3] == 216 && report[4] == 255 && report[5] == 224);
        }
        QVERIFY(std::all_of(ledSeen.begin(), ledSeen.end(), [](bool seen) { return seen; }));
        QVERIFY(leftHead); // the almost white head, on the left ring (LEDs 4 to 7)
    }

    void theRainOnTheRingsFollowsTheLevelOfTheRings()
    {
        Fixture f;
        f.rings = {.lit = true, .level = 500};
        const QSignalSpy active(f.controller.get(), &MatrixRainController::activeChanged);
        f.transport->featureReports.clear();
        f.transport->pendingReads.push_back(dialButtons(true, true));
        QTRY_COMPARE_WITH_TIMEOUT(active.count(), 1, 3000);
        QTRY_VERIFY_WITH_TIMEOUT(ringReports(*f.transport).size() >= 8, 3000);
        bool halfHead = false;
        for (const auto &report : ringReports(*f.transport)) {
            // Nothing is brighter than half of the brightest colour of the rain (the almost white head, 255 at most).
            QVERIFY2(report[3] <= 108 && report[4] <= 128 && report[5] <= 112, "a segment was sent above the level of the rings");
            halfHead = halfHead || (report[3] == 108 && report[4] == 128 && report[5] == 112); // the head at half
        }
        QVERIFY(halfHead);
    }

    void withTheRingsPutOutTheRainRelightsThemAtTheMinimum()
    {
        Fixture f;
        f.rings = {.lit = false, .level = 1000};
        const QSignalSpy active(f.controller.get(), &MatrixRainController::activeChanged);
        f.transport->featureReports.clear();
        f.transport->pendingReads.push_back(dialButtons(true, true));
        QTRY_COMPARE_WITH_TIMEOUT(active.count(), 1, 3000);
        QTRY_VERIFY_WITH_TIMEOUT(ringReports(*f.transport).size() >= 8, 3000);
        bool headAtTheMinimum = false;
        for (const auto &report : ringReports(*f.transport)) {
            // The rain is started on purpose: it shows at 30 % even with the rings put out (the head: 216, 255, 224 -> 65, 77, 67), and never above.
            QVERIFY2(report[3] <= 65 && report[4] <= 77 && report[5] <= 67, "a segment was sent above the minimum with the rings put out");
            headAtTheMinimum = headAtTheMinimum || (report[3] == 65 && report[4] == 77 && report[5] == 67);
        }
        QVERIFY(headAtTheMinimum);
    }

    void aLevelChangedWhileTheRainRunsIsFollowedAtOnce()
    {
        Fixture f;
        f.rings = {.lit = true, .level = 200};
        const QSignalSpy active(f.controller.get(), &MatrixRainController::activeChanged);
        f.transport->pendingReads.push_back(dialButtons(true, true));
        QTRY_COMPARE_WITH_TIMEOUT(active.count(), 1, 3000);
        const auto fullHead = [&f] {
            const auto reports = ringReports(*f.transport);
            return std::ranges::any_of(reports, [](const auto &report) { return report[3] == 216 && report[4] == 255 && report[5] == 224; });
        };
        QTRY_VERIFY_WITH_TIMEOUT(!ringReports(*f.transport).empty(), 3000);
        QTest::qWait(800);
        QVERIFY2(!fullHead(), "at a low level the head was shown at its full colour");
        f.transport->featureReports.clear();
        f.rings = {.lit = true, .level = RingLevelFull}; // the slider of the window moved
        // The head comes back at its full colour, on one of the rings, within a few pictures.
        QTRY_VERIFY_WITH_TIMEOUT(fullHead(), 4000);
    }

    void aSegmentIsSentAgainOnlyWhenItsColourChanged()
    {
        const Fixture f;
        const QSignalSpy active(f.controller.get(), &MatrixRainController::activeChanged);
        f.transport->pendingReads.push_back(dialButtons(true, true));
        QTRY_COMPARE_WITH_TIMEOUT(active.count(), 1, 3000);
        QTest::qWait(2500); // a few dozen pictures of the rain
        const auto reports = ringReports(*f.transport);
        QVERIFY(reports.size() > 8);
        std::array<std::array<int, 3>, 8> last{};
        std::array<bool, 8> known{};
        for (const auto &report : reports) {
            const std::array<int, 3> colour{report[3], report[4], report[5]};
            if (known.at(report[2]))
                QVERIFY2(colour != last.at(report[2]), "a segment was sent again with the colour it already had");
            last.at(report[2]) = colour;
            known.at(report[2]) = true;
        }
    }

    void aRingThatCannotBeWrittenDoesNotEndTheRain()
    {
        Fixture f;
        const QSignalSpy active(f.controller.get(), &MatrixRainController::activeChanged);
        // The reports of the rings are refused, and only they (the keep-alive between the pictures still goes through).
        f.transport->refuseFeature = [](std::span<const std::uint8_t> report) { return report.size() > 1 && report[0] == 0x03 && report[1] == 0x24; };
        f.transport->pendingReads.push_back(dialButtons(true, true));
        QTRY_COMPARE_WITH_TIMEOUT(active.count(), 1, 3000);
        QTest::qWait(1500);
        QVERIFY(f.controller->isActive()); // the screen and the keys are what the rain is about
        QCOMPARE(active.count(), 1);
    }

    void theRingsStartFromNothingAtEachNewRain()
    {
        Fixture f;
        const QSignalSpy active(f.controller.get(), &MatrixRainController::activeChanged);
        f.transport->pendingReads.push_back(dialButtons(true, true));
        QTRY_COMPARE_WITH_TIMEOUT(active.count(), 1, 3000);
        QTRY_VERIFY_WITH_TIMEOUT(ringReports(*f.transport).size() >= 8, 3000);
        f.transport->pendingReads.push_back(dialButtons(false, false));
        QTest::qWait(MatrixRainController::HandsFreeGraceMs + 200);
        f.controller->endNow();
        QTRY_COMPARE_WITH_TIMEOUT(active.count(), 2, 3000);
        f.transport->featureReports.clear();
        f.transport->pendingReads.push_back(dialButtons(true, true));
        QTRY_COMPARE_WITH_TIMEOUT(active.count(), 3, 3000);
        QTRY_VERIFY_WITH_TIMEOUT(ringReports(*f.transport).size() >= 8, 3000); // the pages had put the rings back: all of them are sent again
    }

    void nothingStartsWhileInhibited()
    {
        Fixture f;
        f.controller->setInhibited(true);
        const QSignalSpy active(f.controller.get(), &MatrixRainController::activeChanged);
        f.transport->pendingReads.push_back(dialButtons(true, true));
        QTest::qWait(700);
        QCOMPARE(active.count(), 0);
    }

    void aModuleThatDisappearsEndsTheRain()
    {
        Fixture f;
        const QSignalSpy active(f.controller.get(), &MatrixRainController::activeChanged);
        f.transport->pendingReads.push_back(dialButtons(true, true));
        QTRY_COMPARE_WITH_TIMEOUT(active.count(), 1, 3000);
        f.transport->disconnected = true;
        QTRY_COMPARE_WITH_TIMEOUT(active.count(), 2, 3000);
        QVERIFY(!f.controller->isActive());
    }
};

// A GUI application: Qt Quick needs one, even off-screen.
int main(int argc, char *argv[])
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    const QGuiApplication app(argc, argv);
    TestMatrixRain test;
    return QTest::qExec(&test, argc, argv);
}
#include "test_matrixrain.moc"
