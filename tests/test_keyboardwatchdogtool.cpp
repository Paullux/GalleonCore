// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

// The watchdog program itself, run the way it is installed: a copy in a folder nobody can write to, a working folder
// and a home that cannot be written either, so that a program that tried to write a log beside itself (the first Guard
// of the C# solution crashed at start that way, and for two builds nothing showed it) would fail here. It is only ever
// run with --dry-run: it waits for the process as it should, but opens no keyboard, so a keyboard plugged into the
// machine that runs the tests is never touched. Not run on Windows (see tools/packaging/windows/test-setup.ps1 for the
// installed Setup there). Note that a root user can write anywhere: the check has its full force for everybody else.

#include <QDir>
#include <QFile>
#include <QProcess>
#include <QTemporaryDir>
#include <QTest>

namespace {

// A folder that nobody can write to, holding a copy of the program.
struct InstalledCopy
{
    QTemporaryDir dir;
    QString program;

    InstalledCopy()
    {
        program = dir.filePath("galleon-keyboard-watchdog");
        QVERIFY2(QFile::copy(QStringLiteral(WATCHDOG_TOOL), program), "the program could not be copied");
        QFile::setPermissions(program, QFile::ReadOwner | QFile::ExeOwner | QFile::ReadGroup | QFile::ExeGroup | QFile::ReadOther | QFile::ExeOther);
        QFile::setPermissions(dir.path(), QFile::ReadOwner | QFile::ExeOwner);
    }
    ~InstalledCopy()
    {
        QFile::setPermissions(dir.path(), QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner); // so that it can be removed
    }
    InstalledCopy(const InstalledCopy &) = delete;
    InstalledCopy &operator=(const InstalledCopy &) = delete;

    [[nodiscard]] QStringList files() const { return QDir(dir.path()).entryList(QDir::Files | QDir::Hidden); }
    void run(QProcess &process, const QStringList &arguments) const
    {
        QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
        for (const char *name : {"HOME", "XDG_CONFIG_HOME", "XDG_CACHE_HOME", "XDG_DATA_HOME", "XDG_STATE_HOME", "TMPDIR"})
            environment.insert(QString::fromLatin1(name), QStringLiteral("/nonexistent/unwritable"));
        process.setProcessEnvironment(environment);
        process.setWorkingDirectory(dir.path());
        process.start(program, arguments);
    }
};

} // namespace

class TestKeyboardWatchdogTool : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void withoutArgumentsItSaysHowToUseItAndExitsWithOne()
    {
        const InstalledCopy copy;
        QProcess tool;
        copy.run(tool, {});
        QVERIFY(tool.waitForFinished(5000));
        QCOMPARE(tool.exitCode(), 1);
        QVERIFY(tool.readAllStandardError().contains("usage"));
    }

    void fromAReadOnlyFolderItWaitsForTheProcessThenStopsWithoutWritingAnything()
    {
        const InstalledCopy copy;
        const QStringList before = copy.files();
        QProcess writer; // the process it watches
        writer.start(QStringLiteral("sleep"), {QStringLiteral("1")});
        QVERIFY(writer.waitForStarted());

        QProcess tool;
        copy.run(tool, {QStringLiteral("--pid"), QString::number(writer.processId()), QStringLiteral("--dry-run")});
        QVERIFY2(tool.waitForStarted(5000), qPrintable(tool.errorString()));
        // It is still waiting well after it started: it did not crash at start (the lesson), and it has not acted early.
        QVERIFY2(!tool.waitForFinished(400), "the watchdog ended before the process it watches");
        QVERIFY(writer.waitForFinished(5000));
        QVERIFY(tool.waitForFinished(5000));
        QCOMPARE(tool.exitCode(), 0);
        QVERIFY(tool.readAllStandardOutput().contains("dry run"));
        QCOMPARE(copy.files(), before); // nothing was written beside it
    }

    void aProcessThatIsAlreadyGoneIsNotAnErrorItIsTheCaseTheWatchdogExistsFor()
    {
        const InstalledCopy copy;
        QProcess tool;
        // The highest process ID Linux gives is below 2^22: this one is not running.
        copy.run(tool, {QStringLiteral("--pid"), QStringLiteral("4194999"), QStringLiteral("--dry-run")});
        QVERIFY(tool.waitForFinished(5000));
        QCOMPARE(tool.exitCode(), 0);
        QVERIFY(tool.readAllStandardOutput().contains("dry run"));
    }

    void anArgumentItDoesNotUnderstandSendsNothingAndExitsWithOne()
    {
        const InstalledCopy copy;
        for (const QStringList &arguments : {QStringList{QStringLiteral("--pid"), QStringLiteral("0")}, QStringList{QStringLiteral("--pid"), QStringLiteral("12"), QStringLiteral("--force")},
                                             QStringList{QStringLiteral("12")}}) {
            QProcess tool;
            copy.run(tool, arguments);
            QVERIFY(tool.waitForFinished(5000));
            QCOMPARE(tool.exitCode(), 1);
        }
    }
};

QTEST_GUILESS_MAIN(TestKeyboardWatchdogTool)
#include "test_keyboardwatchdogtool.moc"
