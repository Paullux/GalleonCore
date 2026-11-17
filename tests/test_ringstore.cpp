// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

// The settings of the rings of the dials as the settings window holds them: read at the start, written after every change, and the file is the one
// that galleond reads.
#include "RingStore.h"

#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using namespace bosun;

class TestRingStore : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void withoutAFileTheRingsAreLitAtFullLevelAsTheyAlwaysWere()
    {
        const QTemporaryDir dir;
        const RingStore store(dir.filePath("dial-rings.json"));
        QVERIFY(store.lit());
        QCOMPARE(store.level(), 1000);
    }

    void aChangeIsWrittenAtOnceAndReadBackByANewStore()
    {
        const QTemporaryDir dir;
        const QString path = dir.filePath("dial-rings.json");
        {
            RingStore store(path);
            const QSignalSpy changed(&store, &RingStore::changed);
            QVERIFY(store.setLevel(400).isEmpty());
            QVERIFY(store.setLit(false).isEmpty());
            QCOMPARE(changed.count(), 2);
        }
        const auto written = galleond::readRingSettings(path);
        QVERIFY(written.has_value());
        QCOMPARE(written.value_or(galleond::RingSettings{}), (galleond::RingSettings{.lit = false, .level = 400})); // what galleond reads
        const RingStore again(path);
        QVERIFY(!again.lit());
        QCOMPARE(again.level(), 400);
    }

    void aLevelOutOfRangeIsRefusedAndNothingChanges()
    {
        const QTemporaryDir dir;
        const QString path = dir.filePath("dial-rings.json");
        RingStore store(path);
        QVERIFY(!store.setLevel(1001).isEmpty());
        QVERIFY(!store.setLevel(-1).isEmpty());
        QCOMPARE(store.level(), 1000);
        QVERIFY(!QFile::exists(path));
    }

    void settingTheSameValueWritesNothingAndSaysNothing()
    {
        const QTemporaryDir dir;
        const QString path = dir.filePath("dial-rings.json");
        RingStore store(path);
        const QSignalSpy changed(&store, &RingStore::changed);
        QVERIFY(store.setLit(true).isEmpty()); // already lit
        QVERIFY(store.setLevel(1000).isEmpty());
        QCOMPARE(changed.count(), 0);
        QVERIFY(!QFile::exists(path));
    }

    void aDamagedFileGivesTheDefaultsAndIsNotUsed()
    {
        const QTemporaryDir dir;
        const QString path = dir.filePath("dial-rings.json");
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QVERIFY(file.write("{ broken") > 0);
        file.close();
        const RingStore store(path);
        QVERIFY(store.lit());
        QCOMPARE(store.level(), 1000);
    }

    void withoutAPathNothingIsWrittenButTheSettingsAreHeld()
    {
        RingStore store; // a preview run
        QVERIFY(store.setLit(false).isEmpty());
        QVERIFY(!store.lit());
    }
};

QTEST_GUILESS_MAIN(TestRingStore)
#include "test_ringstore.moc"
