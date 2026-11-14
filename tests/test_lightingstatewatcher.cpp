// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

// Bosun's side of lighting-state.json: it reports a fresh state galleond wrote, once, and ignores everything else.

#include "LightingStateWatcher.h"

#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using namespace galleond;
using bosun::LightingStateWatcher;

class TestLightingStateWatcher : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void aFreshStateWrittenAfterTheWatcherStartedIsReportedOnceWithItsDetail()
    {
        const QTemporaryDir dir;
        const QString path = dir.filePath("state.json");
        LightingStateWatcher watcher(path, 100);
        const QSignalSpy changed(&watcher, &LightingStateWatcher::stateChanged);
        QVERIFY(writeLightingState(path, {.state = QStringLiteral("yielded"), .detail = QStringLiteral("the Web Hub is talking"), .time = 150}));
        QTRY_COMPARE(changed.size(), 1);
        QCOMPARE(changed.constFirst().at(0).toString(), QStringLiteral("yielded"));
        QCOMPARE(changed.constFirst().at(1).toString(), QStringLiteral("the Web Hub is talking"));
        watcher.refresh();
        watcher.refresh();
        QCOMPARE(changed.size(), 1);
    }

    void aStateFromAnEarlierSessionIsIgnored()
    {
        const QTemporaryDir dir;
        const QString path = dir.filePath("state.json");
        QVERIFY(writeLightingState(path, {.state = QStringLiteral("holding"), .detail = {}, .time = 50}));
        LightingStateWatcher watcher(path, 100);
        const QSignalSpy changed(&watcher, &LightingStateWatcher::stateChanged);
        watcher.refresh();
        QTest::qWait(100);
        QCOMPARE(changed.size(), 0);
        QVERIFY(!watcher.last());
    }

    void everyNewWriteIsReportedEvenThoughTheFileIsReplacedEachTime()
    {
        const QTemporaryDir dir;
        const QString path = dir.filePath("state.json");
        const LightingStateWatcher watcher(path, 0);
        const QSignalSpy changed(&watcher, &LightingStateWatcher::stateChanged);
        QVERIFY(writeLightingState(path, {.state = QStringLiteral("starting"), .detail = {}, .time = 1}));
        QTRY_COMPARE(changed.size(), 1);
        QVERIFY(writeLightingState(path, {.state = QStringLiteral("holding"), .detail = {}, .time = 2}));
        QTRY_COMPARE(changed.size(), 2);
        QCOMPARE(changed.constLast().at(0).toString(), QStringLiteral("holding"));
    }

    void aFileThatIsNotValidIsIgnoredAndTheNextGoodOneIsReported()
    {
        const QTemporaryDir dir;
        const QString path = dir.filePath("state.json");
        LightingStateWatcher watcher(path, 0);
        const QSignalSpy changed(&watcher, &LightingStateWatcher::stateChanged);
        QFile bad(path);
        QVERIFY(bad.open(QIODevice::WriteOnly));
        bad.write(R"({"version":1,"state":"burning","detail":"","time":9})");
        bad.close();
        QTest::qWait(150);
        watcher.refresh();
        QCOMPARE(changed.size(), 0);
        QVERIFY(writeLightingState(path, {.state = QStringLiteral("off"), .detail = {}, .time = 10}));
        QTRY_COMPARE(changed.size(), 1);
    }
};

QTEST_GUILESS_MAIN(TestLightingStateWatcher)
#include "test_lightingstatewatcher.moc"
