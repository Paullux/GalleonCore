// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

// Whether the lights are off until morning, as the settings window shows it: read from galleond's file, which is untrusted, and never written here.
#include "NightStateStore.h"

#include <NightMode.h>

#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using namespace bosun;

class TestNightStateStore : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void withoutAFileTheLightsAreOn()
    {
        const QTemporaryDir dir;
        const NightStateStore store(dir.filePath("night-mode.json"), [] { return qint64{1800000000}; });
        QVERIFY(!store.active());
        QVERIFY(store.endTime().isEmpty());
    }

    void aNightInTheFileIsShownWithItsEndTime()
    {
        const QTemporaryDir dir;
        const QString path = dir.filePath("night-mode.json");
        QVERIFY(galleond::writeNightUntil(path, 1800003600));
        const NightStateStore store(path, [] { return qint64{1800000000}; });
        QVERIFY(store.active());
        QCOMPARE(store.until(), qint64{1800003600});
        QVERIFY(!store.endTime().isEmpty());
    }

    void aChangeOfTheFileIsSeenAndSaidOnce()
    {
        const QTemporaryDir dir;
        const QString path = dir.filePath("night-mode.json");
        qint64 now = 1800000000;
        NightStateStore store(path, [&now] { return now; });
        const QSignalSpy changed(&store, &NightStateStore::changed);
        QVERIFY(galleond::writeNightUntil(path, 1800003600));
        store.refresh();
        QVERIFY(store.active());
        QCOMPARE(changed.count(), 1);
        store.refresh(); // nothing new
        QCOMPARE(changed.count(), 1);
        QVERIFY(galleond::removeNightFile(path)); // galleond ended the night
        store.refresh();
        QVERIFY(!store.active());
        QCOMPARE(changed.count(), 2);
    }

    void aNightThatHasEndedIsNotShownEvenIfTheFileIsStillThere()
    {
        const QTemporaryDir dir;
        const QString path = dir.filePath("night-mode.json");
        qint64 now = 1800000000;
        QVERIFY(galleond::writeNightUntil(path, 1800000600));
        NightStateStore store(path, [&now] { return now; });
        QVERIFY(store.active());
        now = 1800000700; // ten minutes later, the file was not removed (galleond is not running)
        store.refresh();
        QVERIFY(!store.active());
    }

    void aDamagedOrFarAwayFileShowsNoNight()
    {
        const QTemporaryDir dir;
        const QString path = dir.filePath("night-mode.json");
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("damaged");
        file.close();
        const NightStateStore damaged(path, [] { return qint64{1800000000}; });
        QVERIFY(!damaged.active());
        QVERIFY(galleond::writeNightUntil(path, 1800000000 + 5 * galleond::MaxNightSeconds));
        const NightStateStore far(path, [] { return qint64{1800000000}; });
        QVERIFY(!far.active());
    }

    void withoutAPathThereIsNeverANight()
    {
        const NightStateStore store;
        QVERIFY(!store.active());
    }

    void thisWindowNeverWritesTheFile()
    {
        const QTemporaryDir dir;
        const QString path = dir.filePath("night-mode.json");
        NightStateStore store(path, [] { return qint64{1800000000}; });
        store.refresh();
        QVERIFY(!QFile::exists(path));
    }
};

QTEST_MAIN(TestNightStateStore)
#include "test_nightstatestore.moc"
