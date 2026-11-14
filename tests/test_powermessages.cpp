// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

// What the window messages of Windows mean for the live lighting: sleep and the end of the session give the keys back, a wake
// takes them again, everything else is ignored. The numbers are those of the Windows SDK; the listening itself is native code
// that only Windows runs.

#include "PowerMessages.h"

#include <QTest>

using namespace galleond;

class TestPowerMessages : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void aSuspensionGivesTheKeysBack()
    {
        QCOMPARE(powerChangeFor(0x0218, 0x0004), PowerChange::GoingToSleep); // WM_POWERBROADCAST, PBT_APMSUSPEND
    }

    void bothKindsOfResumeTakeTheKeysAgain()
    {
        QCOMPARE(powerChangeFor(0x0218, 0x0012), PowerChange::Resumed); // PBT_APMRESUMEAUTOMATIC
        QCOMPARE(powerChangeFor(0x0218, 0x0007), PowerChange::Resumed); // PBT_APMRESUMESUSPEND: the user is back
    }

    void aSessionThatReallyEndsGivesTheKeysBackButTheQuestionAndACancelDoNot()
    {
        QCOMPARE(powerChangeFor(0x0016, 1), PowerChange::GoingToSleep); // WM_ENDSESSION, TRUE
        QCOMPARE(powerChangeFor(0x0016, 0), PowerChange::None);         // the end was cancelled
        QCOMPARE(powerChangeFor(0x0011, 1), PowerChange::None);         // WM_QUERYENDSESSION is only a question
    }

    void otherPowerEventsAndOtherMessagesAreIgnored()
    {
        QCOMPARE(powerChangeFor(0x0218, 0x0000), PowerChange::None); // PBT_APMQUERYSUSPEND
        QCOMPARE(powerChangeFor(0x0218, 0x000A), PowerChange::None); // PBT_APMPOWERSTATUSCHANGE
        QCOMPARE(powerChangeFor(0x0218, 0x8013), PowerChange::None); // PBT_POWERSETTINGCHANGE
        QCOMPARE(powerChangeFor(0x0004, 0x0004), PowerChange::None); // another message with the same wParam
        QCOMPARE(powerChangeFor(0x0000, 0x0000), PowerChange::None);
    }
};

QTEST_GUILESS_MAIN(TestPowerMessages)
#include "test_powermessages.moc"
