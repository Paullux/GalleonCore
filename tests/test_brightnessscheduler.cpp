// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

// The brightness curve applied to a fake keyboard: when it reads, when it writes, and when it leaves the keyboard
// alone. The clock is a variable of the test and the timer is set so long that only evaluate() runs; the
// keyboard is the fake of FakeKeyboard.h. Never opens real hardware.

#include "BrightnessScheduler.h"
#include "FakeKeyboard.h"
#include "KeyboardController.h"

#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QFile>
#include <QRegularExpression>
#include <QTime>

#include <memory>
#include <cstddef>
#include <optional>
#include <vector>

using namespace galleond;
using fakekeyboard::Keyboard;
using fakekeyboard::openerFor;

namespace {

BrightnessCurve curveWith(BrightnessCurve::Mode mode, int night = 200, int morning = 800)
{
    BrightnessCurve curve;
    curve.mode = mode;
    curve.hours.fill(night);
    for (int hour = 8; hour < 20; ++hour)
        curve.hours.at(static_cast<std::size_t>(hour)) = morning;
    return curve;
}

struct Rig
{
    QTemporaryDir dir;
    QString path = dir.filePath("brightness-curve.json");
    std::shared_ptr<Keyboard> state = std::make_shared<Keyboard>();
    std::shared_ptr<QTime> now = std::make_shared<QTime>(3, 0);
    KeyboardController controller{openerFor(state)};
    std::unique_ptr<BrightnessScheduler> scheduler;

    void start()
    {
        const auto clock = now;
        scheduler = std::make_unique<BrightnessScheduler>(controller, path, [clock] { return *clock; }, 3600000);
    }
    // Waits until the controller has no work left: the scheduler's read, and its write if any, are done.
    void settle() const { QTest::qWait(900); }
    [[nodiscard]] int opens() const { return state->opens; }
};

} // namespace

class TestBrightnessScheduler : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void theSliderModeNeverTouchesTheKeyboard()
    {
        Rig rig;
        QVERIFY(writeBrightnessCurve(rig.path, curveWith(BrightnessCurve::Mode::Slider)));
        rig.start();
        rig.scheduler->evaluate();
        rig.settle();
        QCOMPARE(rig.opens(), 0);
        QVERIFY(!rig.scheduler->applied());
    }

    void withoutAFileNothingIsReadOrWritten()
    {
        Rig rig;
        rig.start();
        rig.scheduler->evaluate();
        rig.settle();
        QCOMPARE(rig.opens(), 0);
        QVERIFY(!rig.scheduler->curve());
    }

    void theCurveReadsTheKeyboardThenWritesThePointOfTheHourWhenItDiffers()
    {
        Rig rig;
        rig.state->shownValue = 500;
        QVERIFY(writeBrightnessCurve(rig.path, curveWith(BrightnessCurve::Mode::Curve))); // 03:00 -> 200
        rig.start();
        QTRY_COMPARE_WITH_TIMEOUT(rig.scheduler->applied(), std::optional<int>(200), 4000);
        QCOMPARE(rig.state->setValues, std::vector<int>{200});
        QCOMPARE(rig.state->shownValue, 200);
    }

    void aKeyboardThatAlreadyHoldsThePointIsNotWritten()
    {
        Rig rig;
        rig.state->shownValue = 200; // already what the curve wants
        QVERIFY(writeBrightnessCurve(rig.path, curveWith(BrightnessCurve::Mode::Curve)));
        rig.start();
        QTRY_COMPARE_WITH_TIMEOUT(rig.scheduler->applied(), std::optional<int>(200), 4000);
        QVERIFY(rig.state->setValues.empty()); // read only: no needless write to the keyboard's memory
    }

    void nothingIsRewrittenWhileTheTargetStaysTheSame()
    {
        Rig rig;
        QVERIFY(writeBrightnessCurve(rig.path, curveWith(BrightnessCurve::Mode::Curve)));
        rig.start();
        QTRY_COMPARE_WITH_TIMEOUT(rig.scheduler->applied(), std::optional<int>(200), 4000);
        const int opensAfterFirst = rig.opens();
        for (int tick = 0; tick < 5; ++tick) {
            rig.now->setHMS(3, 10 + tick * 5, 0);
            rig.scheduler->evaluate();
        }
        rig.settle();
        QCOMPARE(rig.opens(), opensAfterFirst); // the keyboard was not even opened
    }

    void theCurveDoesNotFightAChangeMadeInTheMeantimeUntilItsNextPoint()
    {
        Rig rig;
        QVERIFY(writeBrightnessCurve(rig.path, curveWith(BrightnessCurve::Mode::Curve)));
        rig.start();
        QTRY_COMPARE_WITH_TIMEOUT(rig.scheduler->applied(), std::optional<int>(200), 4000);
        rig.state->shownValue = 900; // the user moved the slider, or the Web Hub did
        rig.now->setHMS(3, 40, 0);
        rig.scheduler->evaluate();
        rig.settle();
        QCOMPARE(rig.state->shownValue, 900); // left alone
        QCOMPARE(rig.state->setValues, std::vector<int>{200});
    }

    void theNextHoursPointIsWrittenOnce()
    {
        Rig rig;
        QVERIFY(writeBrightnessCurve(rig.path, curveWith(BrightnessCurve::Mode::Curve)));
        rig.start();
        QTRY_COMPARE_WITH_TIMEOUT(rig.scheduler->applied(), std::optional<int>(200), 4000);
        rig.now->setHMS(7, 31, 0); // nearest point: 08:00 -> 800
        rig.scheduler->evaluate();
        QTRY_COMPARE_WITH_TIMEOUT(rig.scheduler->applied(), std::optional<int>(800), 4000);
        QCOMPARE(rig.state->setValues, (std::vector<int>{200, 800}));
        rig.now->setHMS(7, 50, 0);
        rig.scheduler->evaluate();
        rig.settle();
        QCOMPARE(rig.state->setValues.size(), std::size_t{2});
    }

    void aKeyboardThatCannotBeReadIsNotWrittenAndIsTriedAgainAtTheNextTick()
    {
        Rig rig;
        rig.state->available = false;
        QVERIFY(writeBrightnessCurve(rig.path, curveWith(BrightnessCurve::Mode::Curve)));
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression("could not read the keyboard"));
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression("keyboard brightness not read"));
        rig.start();
        rig.settle();
        QVERIFY(rig.state->setValues.empty());
        QVERIFY(!rig.scheduler->applied());
        rig.state->available = true; // plugged back in
        rig.scheduler->evaluate();
        QTRY_COMPARE_WITH_TIMEOUT(rig.scheduler->applied(), std::optional<int>(200), 4000);
    }

    void switchingToTheSliderStopsTheCurveAndSwitchingBackChecksTheKeyboardAgain()
    {
        Rig rig;
        QVERIFY(writeBrightnessCurve(rig.path, curveWith(BrightnessCurve::Mode::Curve)));
        rig.start();
        QTRY_COMPARE_WITH_TIMEOUT(rig.scheduler->applied(), std::optional<int>(200), 4000);
        QVERIFY(writeBrightnessCurve(rig.path, curveWith(BrightnessCurve::Mode::Slider)));
        QTRY_VERIFY_WITH_TIMEOUT(!rig.scheduler->applied(), 4000);
        const int opens = rig.opens();
        rig.now->setHMS(9, 0, 0);
        rig.scheduler->evaluate();
        rig.settle();
        QCOMPARE(rig.opens(), opens); // the slider is in charge: nothing happens
        QVERIFY(writeBrightnessCurve(rig.path, curveWith(BrightnessCurve::Mode::Curve)));
        QTRY_COMPARE_WITH_TIMEOUT(rig.scheduler->applied(), std::optional<int>(800), 4000);
    }

    void aDamagedFileLeavesTheCurveInForce()
    {
        Rig rig;
        QVERIFY(writeBrightnessCurve(rig.path, curveWith(BrightnessCurve::Mode::Curve)));
        rig.start();
        QTRY_COMPARE_WITH_TIMEOUT(rig.scheduler->applied(), std::optional<int>(200), 4000);
        QFile damaged(rig.path);
        QVERIFY(damaged.open(QIODevice::WriteOnly | QIODevice::Truncate));
        damaged.write(R"({"version":1,"mode":"curve","hours":[)"); // half a file
        damaged.close();
        rig.scheduler->reload();
        QVERIFY(rig.scheduler->curve().has_value());
        QCOMPARE(rig.scheduler->curve().value_or(BrightnessCurve{}).mode, BrightnessCurve::Mode::Curve);
        QCOMPARE(rig.scheduler->applied(), std::optional<int>(200));
    }

    void aCurveSavedAfterStartIsTakenUpWithoutRestarting()
    {
        Rig rig;
        rig.start(); // no file yet
        QVERIFY(writeBrightnessCurve(rig.path, curveWith(BrightnessCurve::Mode::Curve)));
        QTRY_COMPARE_WITH_TIMEOUT(rig.scheduler->applied(), std::optional<int>(200), 6000);
    }
};

QTEST_GUILESS_MAIN(TestBrightnessScheduler)
#include "test_brightnessscheduler.moc"
