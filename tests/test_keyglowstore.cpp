// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

// The choice "the keys glow under your fingers" as Bosun holds it: off unless chosen, written at each change to the file galleond reads, and never offered
// where galleond has no source of key presses.
#include "KeyGlowStore.h"

#include <KeyGlowAccess.h>

#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using namespace bosun;

class TestKeyGlowStore : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void withoutAFileTheGlowIsOff()
    {
        const QTemporaryDir dir;
        const KeyGlowStore store(dir.filePath("key-glow.json"));
        QVERIFY(!store.allow());
    }

    void aDamagedFileMeansOff()
    {
        const QTemporaryDir dir;
        QFile file(dir.filePath("key-glow.json"));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(R"({"version":1,"allow":"yes"})");
        file.close();
        const KeyGlowStore store(dir.filePath("key-glow.json"));
        QVERIFY(!store.allow());
    }

    void aChoiceIsWrittenAtOnceAndReadBackByANewStoreAndByGalleond()
    {
        const QTemporaryDir dir;
        const QString path = dir.filePath("key-glow.json");
        {
            KeyGlowStore store(path);
            const QSignalSpy changed(&store, &KeyGlowStore::changed);
            QVERIFY(store.setAllow(true).isEmpty());
            QCOMPARE(changed.count(), 1);
        }
        QCOMPARE(galleond::keyGlowAllowed(path), true); // what galleond reads
        const KeyGlowStore again(path);
        QVERIFY(again.allow());
    }

    void choosingTheSameThingWritesNothingAndSaysNothing()
    {
        const QTemporaryDir dir;
        const QString path = dir.filePath("key-glow.json");
        KeyGlowStore store(path);
        const QSignalSpy changed(&store, &KeyGlowStore::changed);
        QVERIFY(store.setAllow(false).isEmpty());
        QCOMPARE(changed.count(), 0);
        QVERIFY(!QFile::exists(path));
    }

    void thisChoiceNeverTouchesTheOtherChoices()
    {
        const QTemporaryDir dir;
        KeyGlowStore store(dir.filePath("key-glow.json"));
        QVERIFY(store.setAllow(true).isEmpty());
        QVERIFY(!QFile::exists(dir.filePath("lighting-restore.json")));
        QVERIFY(!QFile::exists(dir.filePath("key-signals.json"))); // the choice of other programs is another file, and is not made by this one
    }

    void whereTheSystemCannotDoItNothingIsChosenWrittenOrOffered()
    {
        const QTemporaryDir dir;
        const QString path = dir.filePath("key-glow.json");
        QVERIFY(galleond::writeKeyGlowAccess(path, true)); // a file that says yes, left by something else, is not taken for a choice here
        KeyGlowStore store(path, false);
        QVERIFY(!store.supported());
        QVERIFY(!store.allow());
        const QSignalSpy changed(&store, &KeyGlowStore::changed);
        QVERIFY(!store.setAllow(true).isEmpty());
        QVERIFY(!store.allow());
        QCOMPARE(changed.count(), 0);
        QVERIFY(galleond::keyGlowAllowed(path)); // and the file is left as it was
    }

    void theChoiceIsOfferedOnLinuxAndNotOnWindowsByDefault()
    {
#ifdef Q_OS_WIN
        QVERIFY(!KeyGlowStore::defaultSupported());
#else
        QVERIFY(KeyGlowStore::defaultSupported());
#endif
    }

    void withoutAPathNothingIsWritten()
    {
        KeyGlowStore store;
        QVERIFY(store.setAllow(true).isEmpty());
        QVERIFY(store.allow());
    }

    void aFileThatCannotBeWrittenIsSaidAndTheChoiceIsKeptInMemory()
    {
        const QTemporaryDir dir;
        KeyGlowStore store(dir.filePath("no-such-folder/inside/a/file/that/is/a/file.json/")); // a path that cannot be a file
        QVERIFY(!store.setAllow(true).isEmpty());
        QVERIFY(store.allow());
    }
};

QTEST_MAIN(TestKeyGlowStore)
#include "test_keyglowstore.moc"
