// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

// The choice "other programs may light single keys" as Bosun holds it: off unless chosen, written at each change to the file galleond reads,
#include "KeySignalsStore.h"

#include <KeySignalsAccess.h>

#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using namespace bosun;

class TestKeySignalsStore : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void withoutAFileTheLightingIsNotPutBack()
    {
        const QTemporaryDir dir;
        const KeySignalsStore store(dir.filePath("key-signals.json"));
        QVERIFY(!store.allow());
    }

    void aDamagedFileMeansOff()
    {
        const QTemporaryDir dir;
        QFile file(dir.filePath("key-signals.json"));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(R"({"version":1,"allow":"yes"})");
        file.close();
        const KeySignalsStore store(dir.filePath("key-signals.json"));
        QVERIFY(!store.allow());
    }

    void aChoiceIsWrittenAtOnceAndReadBackByANewStoreAndByGalleond()
    {
        const QTemporaryDir dir;
        const QString path = dir.filePath("key-signals.json");
        {
            KeySignalsStore store(path);
            const QSignalSpy changed(&store, &KeySignalsStore::changed);
            QVERIFY(store.setAllow(true).isEmpty());
            QCOMPARE(changed.count(), 1);
        }
        QCOMPARE(galleond::keySignalsAllowed(path), true); // what galleond reads
        const KeySignalsStore again(path);
        QVERIFY(again.allow());
    }

    void choosingTheSameThingWritesNothingAndSaysNothing()
    {
        const QTemporaryDir dir;
        const QString path = dir.filePath("key-signals.json");
        KeySignalsStore store(path);
        const QSignalSpy changed(&store, &KeySignalsStore::changed);
        QVERIFY(store.setAllow(false).isEmpty());
        QCOMPARE(changed.count(), 0);
        QVERIFY(!QFile::exists(path));
    }

    void thisChoiceNeverTouchesTheChoiceOfPuttingTheLightingBack()
    {
        const QTemporaryDir dir;
        KeySignalsStore store(dir.filePath("key-signals.json"));
        QVERIFY(store.setAllow(true).isEmpty());
        QVERIFY(!QFile::exists(dir.filePath("lighting-restore.json")));
    }

    void withoutAPathNothingIsWritten()
    {
        KeySignalsStore store;
        QVERIFY(store.setAllow(true).isEmpty());
        QVERIFY(store.allow());
    }

    void aFileThatCannotBeWrittenIsSaidAndTheChoiceIsKeptInMemory()
    {
        const QTemporaryDir dir;
        KeySignalsStore store(dir.filePath("no-such-folder/inside/a/file/that/is/a/file.json/")); // a path that cannot be a file
        QVERIFY(!store.setAllow(true).isEmpty());
        QVERIFY(store.allow());
    }
};

QTEST_MAIN(TestKeySignalsStore)
#include "test_keysignalsstore.moc"
