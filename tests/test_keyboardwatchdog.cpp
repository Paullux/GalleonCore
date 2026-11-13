// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

// The parts of the watchdog tool that are not the keyboard: its arguments and the wait for a process to end.
// Never opens real hardware. The wait is tested on Linux only (a pidfd on a real child process); the Windows
// implementation is built there and still has to be tried on a real machine.

#include "ProcessWait.h"
#include "WatchdogArgs.h"

#include <QProcess>
#include <QTest>
#include <QElapsedTimer>

#include <string_view>
#include <vector>
#include <cstdint>
#include <optional>
#include <string>

using namespace watchdog;

namespace {

std::optional<std::uint32_t> parse(std::initializer_list<std::string_view> arguments)
{
    const std::vector<std::string_view> list(arguments);
    return parseArguments(list);
}

} // namespace

class TestKeyboardWatchdog : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void aProcessIdIsAcceptedAfterPid()
    {
        QCOMPARE(parse({"--pid", "1234"}), std::optional<std::uint32_t>(1234));
        QCOMPARE(parse({"--pid", "1"}), std::optional<std::uint32_t>(1));
        QCOMPARE(parse({"--pid", "4294967295"}), std::optional<std::uint32_t>(4294967295U));
    }

    void anythingElseIsRefusedSoNothingIsSentOnArgumentsItDoesNotUnderstand()
    {
        QVERIFY(!parse({}));
        QVERIFY(!parse({"--pid"}));
        QVERIFY(!parse({"1234"}));
        QVERIFY(!parse({"--pid", "1234", "extra"}));
        QVERIFY(!parse({"-p", "1234"}));
        QVERIFY(!parse({"--PID", "1234"}));
        QVERIFY(!parse({"--pid", "0"}));
        QVERIFY(!parse({"--pid", "-5"}));
        QVERIFY(!parse({"--pid", "+5"}));
        QVERIFY(!parse({"--pid", " 5"}));
        QVERIFY(!parse({"--pid", "5 "}));
        QVERIFY(!parse({"--pid", "12x"}));
        QVERIFY(!parse({"--pid", "0x10"}));
        QVERIFY(!parse({"--pid", ""}));
        QVERIFY(!parse({"--pid", "4294967296"}));
        QVERIFY(!parse({"--pid", "99999999999999999999999"}));
    }

#ifdef Q_OS_LINUX
    void theWaitReturnsOnceTheProcessHasEnded()
    {
        QProcess child;
        child.start("sleep", {"0.4"});
        QVERIFY(child.waitForStarted());
        QElapsedTimer clock;
        clock.start();
        // The test is not the parent of nothing here: the wait must work on any process, child or not.
        QCOMPARE(waitForExit(static_cast<std::uint32_t>(child.processId())), WaitResult::Exited);
        QVERIFY2(clock.elapsed() >= 300, "it must not return before the process ended");
        child.waitForFinished();
    }

    void aProcessThatDoesNotExistIsAlreadyGone()
    {
        // The highest ID Linux gives is below 2^22: this one cannot be running.
        QCOMPARE(waitForExit(4194304U + 1000U), WaitResult::AlreadyGone);
    }
#endif
};

QTEST_GUILESS_MAIN(TestKeyboardWatchdog)
#include "test_keyboardwatchdog.moc"
