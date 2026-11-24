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
        QVERIFY(galleond::writeKeyGlowChoice(path, {true, galleon::keyboard::DefaultGlowColour})); // a file that says yes, left by something else, is not taken for a choice here
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

    void theGlowIsCyanUntilAnotherColourIsChosen()
    {
        const QTemporaryDir dir;
        const KeyGlowStore store(dir.filePath("key-glow.json"));
        QCOMPARE(store.colour(), QStringLiteral("#00c8ff"));
    }

    void aColourIsWrittenWithTheRestOfTheChoiceAndReadBackByANewStoreAndByGalleond()
    {
        const QTemporaryDir dir;
        const QString path = dir.filePath("key-glow.json");
        {
            KeyGlowStore store(path);
            const QSignalSpy changed(&store, &KeyGlowStore::changed);
            QVERIFY(store.setAllow(true).isEmpty());
            QVERIFY(store.setColour("#FF9800").isEmpty()); // in capitals: written as galleond reads it
            QCOMPARE(changed.count(), 2);
            QCOMPARE(store.colour(), QStringLiteral("#ff9800"));
        }
        const auto choice = galleond::keyGlowChoice(path); // what galleond reads
        QVERIFY(choice.allow);
        QCOMPARE(galleond::keyGlowColourText(choice.colour), QStringLiteral("#ff9800"));
        const KeyGlowStore again(path);
        QVERIFY(again.allow());
        QCOMPARE(again.colour(), QStringLiteral("#ff9800"));
    }

    void switchingTheGlowOffKeepsTheColourAndChoosingAColourDoesNotSwitchItOn()
    {
        const QTemporaryDir dir;
        const QString path = dir.filePath("key-glow.json");
        KeyGlowStore store(path);
        QVERIFY(store.setColour("#e53935").isEmpty());
        QVERIFY(!store.allow()); // choosing a colour is not choosing the glow: nothing reads the keys
        QVERIFY(!galleond::keyGlowAllowed(path));
        QVERIFY(store.setAllow(true).isEmpty());
        QVERIFY(store.setAllow(false).isEmpty());
        QCOMPARE(store.colour(), QStringLiteral("#e53935"));
        QCOMPARE(galleond::keyGlowChoice(path).colour, (galleon::keyboard::Rgb{0xe5, 0x39, 0x35}));
    }

    void aTextThatIsNotAColourChangesNothingAndSaysWhy_data()
    {
        QTest::addColumn<QString>("text");
        QTest::newRow("no hash") << "ff9800";
        QTest::newRow("short") << "#f90";
        QTest::newRow("a name") << "orange";
        QTest::newRow("not hexadecimal") << "#gg9800";
        QTest::newRow("a script") << (QString("#ff9800") + QChar(0x22) + QString(", ") + QChar(0x22) + "allow" + QChar(0x22) + ": true");
        QTest::newRow("empty") << "";
    }
    void aTextThatIsNotAColourChangesNothingAndSaysWhy()
    {
        QFETCH(const QString, text);
        const QTemporaryDir dir;
        const QString path = dir.filePath("key-glow.json");
        KeyGlowStore store(path);
        const QSignalSpy changed(&store, &KeyGlowStore::changed);
        QVERIFY(!store.setColour(text).isEmpty());
        QCOMPARE(store.colour(), QStringLiteral("#00c8ff"));
        QCOMPARE(changed.count(), 0);
        QVERIFY(!QFile::exists(path));
    }

    void choosingTheSameColourWritesNothing()
    {
        const QTemporaryDir dir;
        const QString path = dir.filePath("key-glow.json");
        KeyGlowStore store(path);
        const QSignalSpy changed(&store, &KeyGlowStore::changed);
        QVERIFY(store.setColour("#00c8ff").isEmpty());
        QCOMPARE(changed.count(), 0);
        QVERIFY(!QFile::exists(path));
    }

    void whereTheSystemCannotDoItNoColourIsChosenEither()
    {
        const QTemporaryDir dir;
        KeyGlowStore store(dir.filePath("key-glow.json"), false);
        QVERIFY(!store.setColour("#ff9800").isEmpty());
        QCOMPARE(store.colour(), QStringLiteral("#00c8ff"));
        QVERIFY(!QFile::exists(dir.filePath("key-glow.json")));
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
