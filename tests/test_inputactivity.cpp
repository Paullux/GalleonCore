// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

// Noticing that something was done at the computer, from the idle time and nothing else: the decision is pure, and the watcher is tested with a reader that
// gives the idle times we choose (no desktop, no D-Bus, nothing real is read).

#include "InputActivity.h"

#include <QSignalSpy>
#include <QTest>

#include <deque>

using namespace galleond;

class TestInputActivity : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void theIdleTimeStartingAgainIsAnAction()
    {
        IdleActivityDetector detector;
        QVERIFY(!detector.onReading(5000));  // the first reading is only remembered
        QVERIFY(!detector.onReading(5250));  // the idle time grows: nothing was done
        QVERIFY(!detector.onReading(5500));
        QVERIFY(detector.onReading(120));    // it fell: an action
        QVERIFY(!detector.onReading(370));   // and grows again
        QVERIFY(detector.onReading(0));
    }

    void aSmallInexactnessIsNotAnAction()
    {
        IdleActivityDetector detector;
        QVERIFY(!detector.onReading(5000));
        QVERIFY(!detector.onReading(4960)); // a reading a little short of the last one (the clock is not exact): not an action
        QVERIFY(!detector.onReading(5000 - IdleActivityDetector::SlackMs));
        QVERIFY(detector.onReading(5000 - 2 * IdleActivityDetector::SlackMs - 1));
    }

    void resettingForgetsTheLastReading()
    {
        IdleActivityDetector detector;
        QVERIFY(!detector.onReading(9000));
        detector.reset();
        QVERIFY(!detector.onReading(10)); // the first reading after a reset is only a baseline
    }

    void theWatcherSaysActivityOnceThenStopsAndKeepsNothingOfWhatWasDone()
    {
        std::deque<std::optional<qint64>> readings{5000, 5250, 5500, 40, 290};
        InputActivityWatcher watcher([&readings](const std::function<void(std::optional<qint64>)> &done) {
            done(readings.empty() ? std::optional<qint64>(99999) : readings.front());
            if (!readings.empty())
                readings.pop_front();
        }, 10);
        const QSignalSpy activity(&watcher, &InputActivityWatcher::activity);
        QVERIFY(!watcher.watching());
        watcher.start();
        QVERIFY(watcher.watching());
        QTRY_COMPARE_WITH_TIMEOUT(activity.count(), 1, 3000);
        QVERIFY(!watcher.watching()); // it stopped by itself
        QTest::qWait(150);
        QCOMPARE(activity.count(), 1); // and said it once
    }

    void aWatcherThatWasNotStartedReadsNothing()
    {
        int reads = 0;
        InputActivityWatcher watcher([&reads](const std::function<void(std::optional<qint64>)> &done) {
            ++reads;
            done(1);
        }, 10);
        QTest::qWait(150);
        QCOMPARE(reads, 0); // nothing is looked at unless it is started (the rain runs for minutes at most)
        watcher.start();
        watcher.stop();
        const int afterStop = reads;
        QTest::qWait(150);
        QCOMPARE(reads, afterStop); // and nothing once it is stopped
    }

    void aReadingThatArrivesAfterStopIsNotForTheNextWatch()
    {
        std::function<void(std::optional<qint64>)> late;
        int calls = 0;
        InputActivityWatcher watcher([&](const std::function<void(std::optional<qint64>)> &done) {
            ++calls;
            if (calls == 1)
                late = done; // the first reading is slow
            else
                done(100000); // the next ones are quiet
        }, 10);
        const QSignalSpy activity(&watcher, &InputActivityWatcher::activity);
        watcher.start();
        watcher.stop();
        watcher.start();
        QTest::qWait(60);
        late(0); // the slow reading of the first watch comes now, with a low idle time: it must be ignored
        QTest::qWait(60);
        QCOMPARE(activity.count(), 0);
    }

    void aSystemThatGivesNoIdleTimeIsSaidOnceAndNothingIsStopped()
    {
        InputActivityWatcher watcher([](const std::function<void(std::optional<qint64>)> &done) { done(std::nullopt); }, 10);
        const QSignalSpy unavailable(&watcher, &InputActivityWatcher::unavailable);
        const QSignalSpy activity(&watcher, &InputActivityWatcher::activity);
        watcher.start();
        QTest::qWait(150);
        QCOMPARE(unavailable.count(), 1); // once, not at every reading
        QCOMPARE(activity.count(), 0);
        QVERIFY(watcher.watching()); // it keeps trying: the rain still ends on its own limit
    }
};

QTEST_GUILESS_MAIN(TestInputActivity)
#include "test_inputactivity.moc"
