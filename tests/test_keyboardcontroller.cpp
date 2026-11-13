// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

// KeyboardController with a fake keyboard that answers like the real one (a set echo, then the value on a get).
// Never opens real hardware: every opener here returns a fake.

#include "FakeKeyboard.h"
#include "KeyboardController.h"

#include <galleon/Transport.h>
#include <galleon/keyboard/BragiFrame.h>

#include <QSignalSpy>
#include <QTest>

#include <deque>
#include <memory>
#include <mutex>
#include <vector>

using namespace galleond;
using namespace galleon::keyboard;

using fakekeyboard::Keyboard;
using fakekeyboard::openerFor;

class TestKeyboardController : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void onlyWholeStepsOfOneHundredFromZeroToAThousandAreAccepted()
    {
        for (const int ok : {0, 100, 500, 1000})
            QCOMPARE(KeyboardController::acceptedBrightness(ok), std::optional<std::uint16_t>(ok));
        for (const int bad : {-100, -1, 1, 50, 150, 999, 1001, 1100, 100000})
            QVERIFY(!KeyboardController::acceptedBrightness(bad).has_value());
    }

    void aRefusedValueIsNeverSentAndTheKeyboardIsNotOpened()
    {
        const auto state = std::make_shared<Keyboard>();
        KeyboardController controller(openerFor(state));
        const QSignalSpy written(&controller, &KeyboardController::brightnessWritten);
        QVERIFY(!controller.SetBrightness(150));
        QVERIFY(!controller.SetBrightness(1100));
        QTest::qWait(50);
        QCOMPARE(written.size(), 0);
        QCOMPARE(state->opens, 0);
    }

    void nothingIsOpenedUntilARequestIsMade()
    {
        const auto state = std::make_shared<Keyboard>();
        const KeyboardController controller(openerFor(state));
        QTest::qWait(50);
        QCOMPARE(state->opens, 0);
    }

    void aRequestWritesTheValueReadsItBackAndClosesTheKeyboard()
    {
        const auto state = std::make_shared<Keyboard>();
        KeyboardController controller(openerFor(state));
        const QSignalSpy written(&controller, &KeyboardController::brightnessWritten);
        QVERIFY(controller.SetBrightness(300));
        QTRY_COMPARE(written.size(), 1);
        QCOMPARE(written.constFirst().at(0).toInt(), 300);
        QVERIFY(written.constFirst().at(1).toBool());
        QCOMPARE(state->setValues, std::vector<int>{300});
        QCOMPARE(state->opens, 1);
        QCOMPARE(state->closes, 1);
    }

    void aKeyboardThatCannotBeOpenedReportsWhyAndWritesNothing()
    {
        const auto state = std::make_shared<Keyboard>();
        state->available = false;
        KeyboardController controller(openerFor(state));
        const QSignalSpy written(&controller, &KeyboardController::brightnessWritten);
        QVERIFY(controller.SetBrightness(200));
        QTRY_COMPARE(written.size(), 1);
        QVERIFY(!written.constFirst().at(1).toBool());
        QVERIFY(!written.constFirst().at(2).toString().isEmpty());
        QVERIFY(state->setValues.empty());
    }

    void aWriteTheKeyboardDoesNotConfirmIsReportedAsFailed()
    {
        const auto state = std::make_shared<Keyboard>();
        state->answersSet = false; // no echo: the write is never confirmed
        KeyboardController controller(openerFor(state));
        const QSignalSpy written(&controller, &KeyboardController::brightnessWritten);
        QVERIFY(controller.SetBrightness(700));
        QTRY_COMPARE_WITH_TIMEOUT(written.size(), 1, 5000);
        QVERIFY(!written.constFirst().at(1).toBool());
    }

    void requestsMadeWhileOneIsServedKeepOnlyTheNewestAndAreSpacedApart()
    {
        const auto state = std::make_shared<Keyboard>();
        KeyboardController controller(openerFor(state));
        const QSignalSpy written(&controller, &KeyboardController::brightnessWritten);
        QElapsedTimer clock;
        clock.start();
        QVERIFY(controller.SetBrightness(100));
        QVERIFY(controller.SetBrightness(200));
        QVERIFY(controller.SetBrightness(300));
        QVERIFY(controller.SetBrightness(400));
        QTRY_COMPARE_WITH_TIMEOUT(written.size(), 2, 5000);
        // The first request goes at once, then only the newest of the others: 200 and 300 were replaced.
        QCOMPARE(state->setValues, (std::vector<int>{100, 400}));
        QVERIFY2(clock.elapsed() >= KeyboardController::MinIntervalMs, "two writes must be at least MinIntervalMs apart");
    }
    void aReadReportsTheValueTheKeyboardHoldsAndWritesNothing()
    {
        const auto state = std::make_shared<Keyboard>();
        state->shownValue = 450; // set by another program: not a step of 100, reported as it is
        KeyboardController controller(openerFor(state));
        const QSignalSpy read(&controller, &KeyboardController::brightnessRead);
        QVERIFY(controller.ReadBrightness());
        QTRY_COMPARE(read.size(), 1);
        QCOMPARE(read.constFirst().at(0).toInt(), 450);
        QVERIFY(read.constFirst().at(1).toBool());
        QVERIFY(state->setValues.empty());
        QCOMPARE(state->opens, 1);
        QCOMPARE(state->closes, 1);
    }

    void aReadOfAKeyboardThatCannotBeOpenedReportsWhy()
    {
        const auto state = std::make_shared<Keyboard>();
        state->available = false;
        KeyboardController controller(openerFor(state));
        const QSignalSpy read(&controller, &KeyboardController::brightnessRead);
        QVERIFY(controller.ReadBrightness());
        QTRY_COMPARE(read.size(), 1);
        QVERIFY(!read.constFirst().at(1).toBool());
        QVERIFY(!read.constFirst().at(2).toString().isEmpty());
    }

    void aWriteWaitingGoesBeforeAReadAndNeitherIsLost()
    {
        const auto state = std::make_shared<Keyboard>();
        KeyboardController controller(openerFor(state));
        const QSignalSpy written(&controller, &KeyboardController::brightnessWritten);
        const QSignalSpy read(&controller, &KeyboardController::brightnessRead);
        QVERIFY(controller.SetBrightness(100)); // served at once
        QVERIFY(controller.ReadBrightness());   // waits
        QVERIFY(controller.SetBrightness(800)); // waits too, and must go before the read
        QTRY_COMPARE_WITH_TIMEOUT(read.size(), 1, 5000);
        QCOMPARE(written.size(), 2);
        QCOMPARE(read.constFirst().at(0).toInt(), 800); // read after the last write: it sees it
        QCOMPARE(state->setValues, (std::vector<int>{100, 800}));
    }
};

QTEST_GUILESS_MAIN(TestKeyboardController)
#include "test_keyboardcontroller.moc"
