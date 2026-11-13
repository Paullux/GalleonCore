// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

// QuitOnSignals: a signal becomes an event of the event loop (SIGUSR1 here, so that the test is not stopped itself).
// Linux only.

#include "QuitOnSignals.h"

#include <QSignalSpy>
#include <QTest>

#include <csignal>

using namespace galleond;

class TestQuitOnSignals : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void aSignalBecomesAnEventOfTheEventLoopWithItsNumber()
    {
        const QuitOnSignals handler({SIGUSR1});
        const QSignalSpy received(&handler, &QuitOnSignals::signalReceived);
        QCOMPARE(handler.received(), 0);
        QVERIFY(raise(SIGUSR1) == 0);
        QTRY_COMPARE(received.size(), 1);
        QCOMPARE(received.constFirst().constFirst().toInt(), static_cast<int>(SIGUSR1));
        QCOMPARE(handler.received(), 1);
    }

    void severalSignalsAreAllReported()
    {
        const QuitOnSignals handler({SIGUSR1, SIGUSR2});
        const QSignalSpy received(&handler, &QuitOnSignals::signalReceived);
        QVERIFY(raise(SIGUSR1) == 0);
        QVERIFY(raise(SIGUSR2) == 0);
        QVERIFY(raise(SIGUSR1) == 0);
        QTRY_COMPARE(received.size(), 3);
    }

    void aSignalNotListedIsLeftToItsDefaultAction()
    {
        // Only the listed ones are caught: after the handler is gone, the default is back for them too.
        {
            const QuitOnSignals handler({SIGUSR2});
        }
        struct sigaction current {};
        QCOMPARE(sigaction(SIGUSR2, nullptr, &current), 0);
        QVERIFY(current.sa_handler == SIG_DFL);
    }
};

QTEST_GUILESS_MAIN(TestQuitOnSignals)
#include "test_quitonsignals.moc"
