// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#include "DesktopEntries.h"
#include "WellKnownApps.h"

#include <QTest>

using namespace galleond;

class TestWellKnownApps : public QObject
{
    Q_OBJECT
private slots:
    void idSyntaxAcceptsAThemeLikeIdAndRejectsADesktopFileOrAPath()
    {
        QVERIFY(WellKnownApps::isValidIdSyntax(QStringLiteral("discord")));
        QVERIFY(WellKnownApps::isValidIdSyntax(QStringLiteral("not-a-known-app"))); // syntax, not membership
        QVERIFY(!WellKnownApps::isValidIdSyntax(QStringLiteral("discord.desktop"))); // never both at once
        QVERIFY(!WellKnownApps::isValidIdSyntax(QStringLiteral("/usr/share/applications/discord.desktop")));
        QVERIFY(!WellKnownApps::isValidIdSyntax(QStringLiteral("C:/Discord/Discord.exe")));
        QVERIFY(!WellKnownApps::isValidIdSyntax(QStringLiteral("")));
        QVERIFY(!WellKnownApps::isValidIdSyntax(QStringLiteral("Has-Uppercase")));
    }

    void discordIsKnownWithLinuxAndWindowsSignatures()
    {
        QVERIFY(WellKnownApps::knownIds().contains(QStringLiteral("discord")));
        const WellKnownApp *app = WellKnownApps::find(QStringLiteral("discord"));
        QVERIFY(app);
        QVERIFY(app->linuxDesktopIds.contains(QStringLiteral("com.discordapp.Discord.desktop")));
        QVERIFY(app->windowsExeNames.contains(QStringLiteral("Discord.exe")));
        QVERIFY(!WellKnownApps::find(QStringLiteral("not-a-known-app")));
    }

    void windowsSignatureNeedsTheExeNameAndIgnoresMissingMetadata()
    {
        const WellKnownApp &app = *WellKnownApps::find(QStringLiteral("discord"));
        QVERIFY(WellKnownApps::windowsSignatureMatches(QStringLiteral("Discord.exe"), {}, {}, app));
        QVERIFY(WellKnownApps::windowsSignatureMatches(QStringLiteral("discord.exe"), {}, {}, app)); // case-insensitive
        QVERIFY(WellKnownApps::windowsSignatureMatches(QStringLiteral("Discord.exe"), QStringLiteral("Discord"), QStringLiteral("Discord Inc."), app));
        QVERIFY(!WellKnownApps::windowsSignatureMatches(QStringLiteral("NotDiscord.exe"), {}, {}, app));
    }

    void windowsSignatureRejectsAMismatchedProductOrCompanyEvenWithTheRightExeName()
    {
        const WellKnownApp &app = *WellKnownApps::find(QStringLiteral("discord"));
        QVERIFY(!WellKnownApps::windowsSignatureMatches(QStringLiteral("Discord.exe"), QStringLiteral("Totally Unrelated App"), {}, app));
        QVERIFY(!WellKnownApps::windowsSignatureMatches(QStringLiteral("Discord.exe"), {}, QStringLiteral("Some Other Vendor"), app));
    }

    void aKnownPortableAppIdIsAcceptedButAnUnknownOneIsNot()
    {
        // The parser-level gate (PagesConfig, ThemePackage): a theme can write
        // "discord" regardless of which system galleond runs on, but an
        // identifier-shaped string that names nothing in the table is not
        // silently accepted as "maybe valid later" (unlike a literal desktop
        // file ID, where any syntactically correct name passes).
        QVERIFY(DesktopEntries::isValidReference(QStringLiteral("discord")));
        QVERIFY(!DesktopEntries::isValidReference(QStringLiteral("not-a-known-app")));
    }
};
QTEST_MAIN(TestWellKnownApps)
#include "test_wellknownapps.moc"
