// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#include <TrayMenu.h>

#include <QTest>

using bosun::TrayEntry;
using bosun::trayEntries;

namespace {

QList<TrayEntry::Action> actionsOf(const QList<TrayEntry> &entries)
{
    QList<TrayEntry::Action> actions;
    for (const TrayEntry &entry : entries)
        actions << entry.action;
    return actions;
}

} // namespace

class TestTrayMenu : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void theFirstEntryShowsOrHidesTheWindow()
    {
        QCOMPARE(trayEntries(true, true).first().label, QStringLiteral("Hide"));
        QCOMPARE(trayEntries(false, true).first().label, QStringLiteral("Show"));
        QCOMPARE(trayEntries(true, false).first().action, TrayEntry::Action::ToggleWindow);
    }

    void whenGalleondRunsOneCanQuitAndStopItOrQuitAlone()
    {
        const auto entries = trayEntries(true, true);
        QCOMPARE(actionsOf(entries), (QList<TrayEntry::Action>{TrayEntry::Action::ToggleWindow, TrayEntry::Action::QuitAndStop, TrayEntry::Action::Quit}));
        QVERIFY(entries.at(1).label.contains("stop galleond"));
        QVERIFY(entries.at(2).label.contains("keeps running")); // said, so that nobody is surprised
    }

    void whenGalleondDoesNotRunOneCanStartItAndStoppingIsNotOffered()
    {
        const auto entries = trayEntries(false, false);
        QCOMPARE(actionsOf(entries), (QList<TrayEntry::Action>{TrayEntry::Action::ToggleWindow, TrayEntry::Action::StartDaemon, TrayEntry::Action::Quit}));
        QVERIFY(!actionsOf(entries).contains(TrayEntry::Action::QuitAndStop));
    }

    void withSeveralProfilesEachHasAnEntryAndTheActiveOneIsChecked()
    {
        const auto entries = trayEntries(true, true, {"default", "gaming", "work"}, "gaming");
        QCOMPARE(actionsOf(entries), (QList<TrayEntry::Action>{TrayEntry::Action::ToggleWindow, TrayEntry::Action::SetProfile, TrayEntry::Action::SetProfile,
                                                                TrayEntry::Action::SetProfile, TrayEntry::Action::QuitAndStop, TrayEntry::Action::Quit}));
        QCOMPARE(entries.at(1).profile, QStringLiteral("default"));
        QCOMPARE(entries.at(2).profile, QStringLiteral("gaming"));
        QCOMPARE(entries.at(3).profile, QStringLiteral("work"));
        QCOMPARE(entries.at(2).label, QStringLiteral("Profile: gaming"));
        QVERIFY(!entries.at(1).checked);
        QVERIFY(entries.at(2).checked); // the one shown is checked, and only it
        QVERIFY(!entries.at(3).checked);
        QVERIFY(entries.at(0).profile.isEmpty()); // the other entries name no profile
        // The same with galleond not running: the profile can still be chosen (its choice is read at the next start).
        QVERIFY(actionsOf(trayEntries(false, false, {"default", "gaming"}, "default")).contains(TrayEntry::Action::SetProfile));
    }

    void withOneProfileTheMenuStaysAsShortAsItWas()
    {
        QCOMPARE(actionsOf(trayEntries(true, true, {"default"}, "default")), actionsOf(trayEntries(true, true)));
        QCOMPARE(actionsOf(trayEntries(true, true, {}, {})), actionsOf(trayEntries(true, true)));
    }

    void anActiveProfileThatIsNotInTheListChecksNone()
    {
        for (const TrayEntry &entry : trayEntries(true, true, {"default", "gaming"}, "vanished"))
            QVERIFY(!entry.checked);
    }

    void everyEntryHasAWordToShow()
    {
        for (const bool shown : {false, true}) {
            for (const bool running : {false, true}) {
                for (const TrayEntry &entry : trayEntries(shown, running))
                    QVERIFY(!entry.label.isEmpty());
            }
        }
    }
};

QTEST_GUILESS_MAIN(TestTrayMenu)
#include "test_traymenu.moc"
