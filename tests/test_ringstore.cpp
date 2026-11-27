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

    void withoutAFileEachRingHasItsDefaultColour()
    {
        const QTemporaryDir dir;
        const RingStore store(dir.filePath("dial-rings.json"));
        QCOMPARE(store.leftColour(), QStringLiteral("default"));
        QCOMPARE(store.rightColour(), QStringLiteral("default"));
    }

    void aColourChosenForARingIsWrittenAtOnceReadBackAndReadByGalleond()
    {
        const QTemporaryDir dir;
        const QString path = dir.filePath("dial-rings.json");
        {
            RingStore store(path);
            const QSignalSpy changed(&store, &RingStore::changed);
            QVERIFY(store.setLeftColour(QStringLiteral("#1E88E5")).isEmpty()); // upper case is accepted ...
            QVERIFY(store.setRightColour(QStringLiteral("#ffd600")).isEmpty());
            QCOMPARE(changed.count(), 2);
            QCOMPARE(store.leftColour(), QStringLiteral("#1e88e5")); // ... and held in lower case
        }
        const auto written = galleond::readRingSettings(path);
        QVERIFY(written.has_value());
        QCOMPARE(written.value_or(galleond::RingSettings{}), (galleond::RingSettings{.lit = true, .level = 1000, .left = 0x1e88e5U, .right = 0xffd600U}));
        const RingStore again(path);
        QCOMPARE(again.leftColour(), QStringLiteral("#1e88e5"));
        QCOMPARE(again.rightColour(), QStringLiteral("#ffd600"));
    }

    void goingBackToDefaultIsWrittenToo()
    {
        const QTemporaryDir dir;
        const QString path = dir.filePath("dial-rings.json");
        RingStore store(path);
        QVERIFY(store.setLeftColour(QStringLiteral("#43a047")).isEmpty());
        QVERIFY(store.setLeftColour(QStringLiteral("default")).isEmpty());
        QCOMPARE(store.leftColour(), QStringLiteral("default"));
        QVERIFY(!galleond::readRingSettings(path).value_or(galleond::RingSettings{.left = 1U}).left.has_value());
    }

    void aTextThatIsNotAColourIsRefusedAndNothingChangesOrIsWritten()
    {
        const QTemporaryDir dir;
        const QString path = dir.filePath("dial-rings.json");
        RingStore store(path);
        const QSignalSpy changed(&store, &RingStore::changed);
        for (const QString &text : {QString(), QStringLiteral("red"), QStringLiteral("#fff"), QStringLiteral("#12345g"), QStringLiteral("DEFAULT")}) {
            QVERIFY2(!store.setLeftColour(text).isEmpty(), qPrintable(text));
            QVERIFY2(!store.setRightColour(text).isEmpty(), qPrintable(text));
        }
        QCOMPARE(changed.count(), 0);
        QCOMPARE(store.leftColour(), QStringLiteral("default"));
        QVERIFY(!QFile::exists(path));
    }

    void aFileOfBeforeTheColoursGivesTheDefaultColoursAndKeepsItsOtherSettings()
    {
        const QTemporaryDir dir;
        const QString path = dir.filePath("dial-rings.json");
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QVERIFY(file.write(R"({"version":1,"lit":false,"level":300})") > 0);
        file.close();
        RingStore store(path);
        QVERIFY(!store.lit());
        QCOMPARE(store.level(), 300);
        QCOMPARE(store.leftColour(), QStringLiteral("default"));
        QCOMPARE(store.rightColour(), QStringLiteral("default"));
        QVERIFY(store.setRightColour(QStringLiteral("#ab47bc")).isEmpty()); // the next write brings the file to the version with the colours, level and lit kept
        const auto written = galleond::readRingSettings(path);
        QCOMPARE(written.value_or(galleond::RingSettings{}), (galleond::RingSettings{.lit = false, .level = 300, .left = std::nullopt, .right = 0xab47bcU}));
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
