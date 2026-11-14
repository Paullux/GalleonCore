// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

// The retry of a refused save (Windows refuses to replace a file that a reader has open for a moment) and the atomic write
// built on it. The rename that Windows refuses cannot be provoked here: the loop is tested with a try that fails on demand.

#include "AtomicFile.h"

#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QTest>

#include <vector>

using namespace galleond;

class TestAtomicFile : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void aSaveRefusedTwiceAndThenAcceptedIsDoneWithoutWaitingAfterTheSuccess()
    {
        int tries = 0;
        std::vector<int> waits;
        QVERIFY(retryUntilDone(6, [&] { return ++tries == 3; }, [&](int attempt) { waits.push_back(attempt); }));
        QCOMPARE(tries, 3);
        QCOMPARE(waits, (std::vector<int>{1, 2})); // between the tries, never after the one that worked
    }

    void aSaveRefusedEveryTimeGivesUpAfterTheLastTryWithoutWaitingAgain()
    {
        int tries = 0;
        std::vector<int> waits;
        QVERIFY(!retryUntilDone(4, [&] { ++tries; return false; }, [&](int attempt) { waits.push_back(attempt); }));
        QCOMPARE(tries, 4);
        QCOMPARE(waits.size(), std::size_t{3});
    }

    void aSaveThatWorksTheFirstTimeNeverWaits()
    {
        int waits = 0;
        QVERIFY(retryUntilDone(6, [] { return true; }, [&](int) { ++waits; }));
        QCOMPARE(waits, 0);
    }

    void theTotalWaitIsBoundedBecauseTheCallerHasOtherWork()
    {
        QVERIFY(MaxAttempts * RetryDelayMs <= 100);
    }

    void aFileKeepsTheRequestedPermissionsAndAFailureSaysWhy()
    {
        const QTemporaryDir dir;
        const QString path = dir.filePath("pages.json");
        QVERIFY(writeFileAtomically(path, "x", QFile::ReadOwner | QFile::WriteOwner));
#ifndef Q_OS_WIN
        // The bits of the group and of the others mean nothing on Windows, where Qt reports them as set.
        QCOMPARE(QFileInfo(path).permissions() & (QFile::ReadGroup | QFile::ReadOther | QFile::WriteGroup | QFile::WriteOther), QFileDevice::Permissions());
#endif
        // A folder that cannot be made (its parent is a file): false, and the reason is given.
        QString why;
        QVERIFY(!writeFileAtomically(path + "/inside/x.json", "x", std::nullopt, &why));
        QVERIFY(!why.isEmpty());
    }

    void aFileIsWrittenWholeInAFolderThatDidNotExistAndReplacedByTheNextOne()
    {
        const QTemporaryDir dir;
        const QString path = dir.filePath("later/state.json");
        QVERIFY(writeFileAtomically(path, "first"));
        QVERIFY(writeFileAtomically(path, "second"));
        QFile file(path);
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(file.readAll(), QByteArray("second"));
        QCOMPARE(QDir(dir.filePath("later")).entryList(QDir::Files).size(), 1); // no temporary file left
    }
};

QTEST_GUILESS_MAIN(TestAtomicFile)
#include "test_atomicfile.moc"
