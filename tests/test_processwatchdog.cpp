// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

// ProcessWatchdog with small shell scripts in place of galleon-keyboard-watchdog: it believes in a watchdog only if the
// program exists, starts and is still running a moment later, and it says when a wanted one dies. Unix only (the scripts).
// Never touches the keyboard.

#include "ProcessWatchdog.h"

#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using namespace galleond;

namespace {

QString script(const QTemporaryDir &dir, const QString &name, const QString &body, bool executable = true)
{
    const QString path = dir.filePath(name);
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return {};
    file.write(("#!/bin/sh\n" + body + "\n").toUtf8());
    file.close();
    QFile::setPermissions(path, QFile::permissions(path) | (executable ? QFile::ExeUser : QFileDevice::Permissions()));
    return dir.filePath(name);
}

// How many times the scripts logged that they were started for process 4242.
int runsOf(const QString &log)
{
    QFile file(log);
    return file.open(QIODevice::ReadOnly) ? static_cast<int>(file.readAll().count("--pid 4242")) : 0;
}

} // namespace

class TestProcessWatchdog : public QObject
{
    Q_OBJECT

private Q_SLOTS:
#ifdef Q_OS_UNIX
    void aProgramThatIsNotThereIsRefusedWithAReasonAndNothingRuns()
    {
        ProcessWatchdog watchdog(QStringLiteral("/nonexistent/galleon-keyboard-watchdog"));
        QString why;
        QVERIFY(!watchdog.start(1234, &why));
        QVERIFY(why.contains(QStringLiteral("not installed")));
    }

    void aFileThatCannotBeRunIsRefused()
    {
        const QTemporaryDir dir;
        ProcessWatchdog watchdog(script(dir, "watchdog", "exec sleep 30", false));
        QString why;
        QVERIFY(!watchdog.start(1234, &why));
        QVERIFY(!why.isEmpty());
    }

    void aWatchdogThatEndsAtOnceProtectsNothingAndIsRefusedWithItsExitCode()
    {
        const QTemporaryDir dir;
        ProcessWatchdog watchdog(script(dir, "watchdog", "exit 3"));
        const QSignalSpy died(&watchdog, &WatchdogGuard::died);
        QString why;
        QVERIFY(!watchdog.start(1234, &why));
        QVERIFY2(why.contains(QStringLiteral("3")), qPrintable(why));
        QCOMPARE(died.size(), 0); // refused by start(): not a death to report as well
    }

    void aWatchdogThatKeepsRunningIsStartedForTheGivenProcessAndStoppedOnRequest()
    {
        const QTemporaryDir dir;
        const QString arguments = dir.filePath("arguments");
        ProcessWatchdog watchdog(script(dir, "watchdog", "echo \"$@\" > '" + arguments + "'\nexec sleep 30"));
        const QSignalSpy died(&watchdog, &WatchdogGuard::died);
        QString why;
        QVERIFY2(watchdog.start(4242, &why), qPrintable(why));
        QFile written(arguments);
        QVERIFY(written.open(QIODevice::ReadOnly));
        QCOMPARE(written.readAll().trimmed(), QByteArray("--pid 4242"));
        watchdog.stop();
        QTest::qWait(100);
        QCOMPARE(died.size(), 0); // stopping it on purpose is not a death
    }

    void startingAWatchdogThatIsAlreadyRunningChangesNothing()
    {
        const QTemporaryDir dir;
        ProcessWatchdog watchdog(script(dir, "watchdog", "exec sleep 30"));
        QString why;
        QVERIFY(watchdog.start(1, &why));
        QVERIFY(watchdog.start(1, &why));
        watchdog.stop();
    }

    void aWatchdogStillWantedWhenItsObjectGoesIsHandedOverToAnotherThatOutlivesIt()
    {
        const QTemporaryDir dir;
        const QString log = dir.filePath("runs");
        const QString program = script(dir, "watchdog", "echo \"$@\" >> '" + log + "'\nexec sleep 3");
        {
            ProcessWatchdog watchdog(program);
            QString why;
            QVERIFY2(watchdog.start(4242, &why), qPrintable(why));
        } // gone without stop(): the keys were not confirmed back
        QTRY_COMPARE_WITH_TIMEOUT(runsOf(log), 2, 3000); // the first one, and the one that took over
    }

    void aWatchdogStoppedOnPurposeIsNotHandedOver()
    {
        const QTemporaryDir dir;
        const QString log = dir.filePath("runs");
        const QString program = script(dir, "watchdog", "echo \"$@\" >> '" + log + "'\nexec sleep 3");
        {
            ProcessWatchdog watchdog(program);
            QString why;
            QVERIFY2(watchdog.start(4242, &why), qPrintable(why));
            watchdog.stop(); // the keys were confirmed back: it is not needed any more
        }
        QTest::qWait(500);
        QCOMPARE(runsOf(log), 1);
    }

    void aWatchdogThatDiesWhileItIsWantedIsReported()
    {
        const QTemporaryDir dir;
        // It runs a little over the settle time, then ends: it was believed in, and then it is gone.
        ProcessWatchdog watchdog(script(dir, "watchdog", "sleep 1\nexit 0"));
        QSignalSpy died(&watchdog, &WatchdogGuard::died);
        QString why;
        QVERIFY2(watchdog.start(1234, &why), qPrintable(why));
        QVERIFY(died.wait(5000));
        QCOMPARE(died.size(), 1);
    }

    void theDefaultProgramIsNextToTheRunningExecutable()
    {
        QVERIFY(ProcessWatchdog::defaultProgram().contains(QStringLiteral("galleon-keyboard-watchdog")));
    }
#endif
};

QTEST_GUILESS_MAIN(TestProcessWatchdog)
#include "test_processwatchdog.moc"
