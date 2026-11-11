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
