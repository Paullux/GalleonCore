// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

// Bosun's side of keyboard-state.json: it reports a fresh value galleond wrote, once, and ignores everything
// else. The file is replaced by a rename on every write, as galleond does.

#include "KeyboardStateWatcher.h"

#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using namespace galleond;
using bosun::KeyboardStateWatcher;

class TestKeyboardStateWatcher : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void aFreshValueWrittenAfterTheWatcherStartedIsReported()
    {
        const QTemporaryDir dir;
        const QString path = dir.filePath("state.json");
        const KeyboardStateWatcher watcher(path, 100);
        const QSignalSpy changed(&watcher, &KeyboardStateWatcher::brightnessChanged);
        QVERIFY(writeKeyboardState(path, {.brightness = 700, .time = 150}));
        QTRY_COMPARE(changed.size(), 1);
        QCOMPARE(changed.constFirst().constFirst().toInt(), 700);
    }

    void aValueFromAnEarlierSessionIsIgnored()
    {
        const QTemporaryDir dir;
        const QString path = dir.filePath("state.json");
        QVERIFY(writeKeyboardState(path, {.brightness = 700, .time = 50})); // written before the watcher's start (100)
        KeyboardStateWatcher watcher(path, 100);
        const QSignalSpy changed(&watcher, &KeyboardStateWatcher::brightnessChanged);
        watcher.refresh();
        QTest::qWait(100);
        QCOMPARE(changed.size(), 0);
        QVERIFY(!watcher.brightness());
    }

    void aFreshValueAlreadyThereWhenTheWatcherStartsIsFoundAtOnce()
    {
        const QTemporaryDir dir;
        const QString path = dir.filePath("state.json");
        QVERIFY(writeKeyboardState(path, {.brightness = 400, .time = 200}));
        const KeyboardStateWatcher watcher(path, 100);
        QCOMPARE(watcher.brightness(), std::optional<int>(400));
    }

    void everyNewWriteIsReportedEvenThoughTheFileIsReplacedEachTime()
    {
        const QTemporaryDir dir;
        const QString path = dir.filePath("state.json");
        const KeyboardStateWatcher watcher(path, 0);
        const QSignalSpy changed(&watcher, &KeyboardStateWatcher::brightnessChanged);
        QVERIFY(writeKeyboardState(path, {.brightness = 100, .time = 1}));
        QTRY_COMPARE(changed.size(), 1);
        QVERIFY(writeKeyboardState(path, {.brightness = 200, .time = 2}));
        QTRY_COMPARE(changed.size(), 2);
        QVERIFY(writeKeyboardState(path, {.brightness = 300, .time = 3}));
        QTRY_COMPARE(changed.size(), 3);
        QCOMPARE(changed.constLast().constFirst().toInt(), 300);
    }

    void theSameStateReadTwiceIsReportedOnce()
    {
        const QTemporaryDir dir;
        const QString path = dir.filePath("state.json");
        KeyboardStateWatcher watcher(path, 0);
        const QSignalSpy changed(&watcher, &KeyboardStateWatcher::brightnessChanged);
        QVERIFY(writeKeyboardState(path, {.brightness = 500, .time = 4}));
        QTRY_COMPARE(changed.size(), 1);
        watcher.refresh();
        watcher.refresh();
        QCOMPARE(changed.size(), 1);
    }

    void aFileThatIsNotValidIsIgnoredAndTheNextGoodOneIsReported()
    {
        const QTemporaryDir dir;
        const QString path = dir.filePath("state.json");
        KeyboardStateWatcher watcher(path, 0);
        const QSignalSpy changed(&watcher, &KeyboardStateWatcher::brightnessChanged);
        QFile bad(path);
        QVERIFY(bad.open(QIODevice::WriteOnly));
        bad.write(R"({"version":1,"brightness":5000,"time":9})");
        bad.close();
        QTest::qWait(150);
        watcher.refresh();
        QCOMPARE(changed.size(), 0);
        QVERIFY(writeKeyboardState(path, {.brightness = 600, .time = 10}));
        QTRY_COMPARE(changed.size(), 1);
        QCOMPARE(changed.constFirst().constFirst().toInt(), 600);
    }

    void aFolderThatDoesNotExistYetIsWatchedOnceItAppears()
    {
        const QTemporaryDir dir;
        const QString path = dir.filePath("later/state.json");
        const KeyboardStateWatcher watcher(path, 0);
        const QSignalSpy changed(&watcher, &KeyboardStateWatcher::brightnessChanged);
        QVERIFY(writeKeyboardState(path, {.brightness = 250, .time = 5})); // creates the folder
        QTRY_COMPARE_WITH_TIMEOUT(changed.size(), 1, 6000); // found by the retry timer at the latest
        QCOMPARE(changed.constFirst().constFirst().toInt(), 250);
    }
};

QTEST_GUILESS_MAIN(TestKeyboardStateWatcher)
#include "test_keyboardstatewatcher.moc"
