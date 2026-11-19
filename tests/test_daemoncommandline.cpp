// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

// What the galleond program says for --version, --help and a bad option, run as the real program (it opens no device for these).
// --version and --help must end by returning from main(): the process exits normally, with Qt's thread storage still valid, so nothing is
// printed about thread storage ("QThreadStorage: entry N destroyed before end of thread" was reported on Qt 6.10, KDE Plasma). The test looks for that
// warning, not for an empty error output: Qt may write other notices that are not about this (for instance the one about a non UTF-8 locale, which
// the Debian package build, run in the "C" locale, shows).
#include <QProcess>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <QTest>

namespace {

struct Run
{
    int code = -1;
    QProcess::ExitStatus status = QProcess::CrashExit;
    QString out;
    QString err;
};

Run runDaemon(const QStringList &arguments, const QString &runtimeDir)
{
    QProcess process;
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    environment.insert(QStringLiteral("QT_QPA_PLATFORM"), QStringLiteral("offscreen"));
    environment.insert(QStringLiteral("XDG_RUNTIME_DIR"), runtimeDir);
    environment.insert(QStringLiteral("LC_ALL"), QStringLiteral("C.UTF-8")); // no notice about the locale of the build machine
    process.setProcessEnvironment(environment);
    process.start(QStringLiteral(GALLEON_TEST_DAEMON), arguments);
    if (!process.waitForFinished(20000)) {
        process.kill();
        process.waitForFinished(2000);
    }
    return {process.exitCode(), process.exitStatus(), QString::fromUtf8(process.readAllStandardOutput()),
            QString::fromUtf8(process.readAllStandardError())};
}

} // namespace

class TestDaemonCommandLine : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void versionPrintsTheNameAndTheVersionAndNoThreadStorageWarning()
    {
        const QTemporaryDir runtime;
        const Run run = runDaemon({QStringLiteral("--version")}, runtime.path());
        QCOMPARE(run.status, QProcess::NormalExit);
        QCOMPARE(run.code, 0);
        QVERIFY2(QRegularExpression(QStringLiteral("^galleond \\d+\\.\\d+\\.\\d+\\S*\\n$")).match(run.out).hasMatch(), qPrintable(run.out));
        QVERIFY2(!run.err.contains(QStringLiteral("QThreadStorage")), qPrintable(run.err));
    }

    void helpListsTheOptionsAndNoThreadStorageWarning()
    {
        const QTemporaryDir runtime;
        const Run run = runDaemon({QStringLiteral("--help")}, runtime.path());
        QCOMPARE(run.status, QProcess::NormalExit);
        QCOMPARE(run.code, 0);
        QVERIFY2(run.out.contains(QStringLiteral("Usage:")) && run.out.contains(QStringLiteral("--version")), qPrintable(run.out));
        QVERIFY2(!run.err.contains(QStringLiteral("QThreadStorage")), qPrintable(run.err));
    }

    void anUnknownOptionStillFailsAndNamesIt()
    {
        const QTemporaryDir runtime;
        const Run run = runDaemon({QStringLiteral("--no-such-option")}, runtime.path());
        QCOMPARE(run.status, QProcess::NormalExit);
        QVERIFY(run.code != 0);
        QVERIFY2(run.err.contains(QStringLiteral("no-such-option")), qPrintable(run.err));
    }
};

QTEST_GUILESS_MAIN(TestDaemonCommandLine)
#include "test_daemoncommandline.moc"
