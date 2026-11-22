// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

// The choice "put the live lighting back when galleond starts" as Bosun holds it: off unless chosen, written at each change to the file galleond reads,
// and never the file that galleond itself writes.
#include "LightingRestoreStore.h"

#include <LightingRestore.h>

#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using namespace bosun;

class TestLightingRestoreStore : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void withoutAFileTheLightingIsNotPutBack()
    {
        const QTemporaryDir dir;
        const LightingRestoreStore store(dir.filePath("lighting-restore.json"));
        QVERIFY(!store.restore());
    }

    void aDamagedFileMeansOff()
    {
        const QTemporaryDir dir;
        QFile file(dir.filePath("lighting-restore.json"));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(R"({"version":1,"restore":"yes"})");
        file.close();
        const LightingRestoreStore store(dir.filePath("lighting-restore.json"));
        QVERIFY(!store.restore());
    }

    void aChoiceIsWrittenAtOnceAndReadBackByANewStoreAndByGalleond()
    {
        const QTemporaryDir dir;
        const QString path = dir.filePath("lighting-restore.json");
        {
            LightingRestoreStore store(path);
            const QSignalSpy changed(&store, &LightingRestoreStore::changed);
            QVERIFY(store.setRestore(true).isEmpty());
            QCOMPARE(changed.count(), 1);
        }
        QCOMPARE(galleond::readLightingRestore(path).value_or(false), true); // what galleond reads
        const LightingRestoreStore again(path);
        QVERIFY(again.restore());
    }

    void choosingTheSameThingWritesNothingAndSaysNothing()
    {
        const QTemporaryDir dir;
        const QString path = dir.filePath("lighting-restore.json");
        LightingRestoreStore store(path);
        const QSignalSpy changed(&store, &LightingRestoreStore::changed);
        QVERIFY(store.setRestore(false).isEmpty());
        QCOMPARE(changed.count(), 0);
        QVERIFY(!QFile::exists(path));
    }

    void thisWindowNeverWritesWhatGalleondRemembers()
    {
        const QTemporaryDir dir;
        LightingRestoreStore store(dir.filePath("lighting-restore.json"));
        QVERIFY(store.setRestore(true).isEmpty());
        QVERIFY(!QFile::exists(dir.filePath("lighting-memory.json")));
    }

    void withoutAPathNothingIsWritten()
    {
        LightingRestoreStore store;
        QVERIFY(store.setRestore(true).isEmpty());
        QVERIFY(store.restore());
    }

    void aFileThatCannotBeWrittenIsSaidAndTheChoiceIsKeptInMemory()
    {
        const QTemporaryDir dir;
        LightingRestoreStore store(dir.filePath("no-such-folder/inside/a/file/that/is/a/file.json/")); // a path that cannot be a file
        QVERIFY(!store.setRestore(true).isEmpty());
        QVERIFY(store.restore());
    }
};

QTEST_MAIN(TestLightingRestoreStore)
#include "test_lightingrestorestore.moc"
