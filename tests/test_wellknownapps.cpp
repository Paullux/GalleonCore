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

    void theSixAudioAndVideoApplicationsAreKnownWithWhatWasReadOnAWindowsPc()
    {
        struct Row { const char *id; const char *linux; const char *exe; };
        for (const Row &row : {Row{"audacity", "audacity.desktop", "Audacity.exe"}, Row{"audacity", "audacity.desktop", "Audacity4.exe"},
                               Row{"ardour", "ardour.desktop", "Ardour.exe"}, Row{"kdenlive", "org.kde.kdenlive.desktop", "kdenlive.exe"},
                               Row{"lmms", "lmms.desktop", "lmms.exe"}, Row{"musescore", "mscore.desktop", "MuseScore4.exe"},
                               Row{"tuxguitar", "ar.com.tuxguitar.TuxGuitar.desktop", "tuxguitar.exe"}}) {
            const WellKnownApp *app = WellKnownApps::find(QString::fromLatin1(row.id));
            QVERIFY2(app, row.id);
            QVERIFY(app->linuxDesktopIds.contains(QString::fromLatin1(row.linux)));
            QVERIFY(WellKnownApps::windowsSignatureMatches(QString::fromLatin1(row.exe), {}, {}, *app)); // the exe name alone
            QVERIFY(DesktopEntries::isValidReference(QString::fromLatin1(row.id)));
            QVERIFY(WellKnownApps::isValidIdSyntax(QString::fromLatin1(row.id)));
        }
    }

    void audacityThreeAndFourEachMatchAsAWholeAndNeverMixed()
    {
        const WellKnownApp &app = *WellKnownApps::find(QStringLiteral("audacity"));
        QVERIFY(WellKnownApps::windowsSignatureMatches(QStringLiteral("Audacity.exe"), QStringLiteral("Audacity"), QStringLiteral("Audacity Team"), app));
        QVERIFY(WellKnownApps::windowsSignatureMatches(QStringLiteral("Audacity4.exe"), QStringLiteral("Audacity 4.0"), QStringLiteral("MuseCY Holdings Ltd"), app));
        // The product of one version with the publisher of the other, or an unrelated publisher, is not either of them.
        QVERIFY(!WellKnownApps::windowsSignatureMatches(QStringLiteral("Audacity.exe"), QStringLiteral("Audacity 4.0"), QStringLiteral("Audacity Team"), app));
        QVERIFY(!WellKnownApps::windowsSignatureMatches(QStringLiteral("Audacity.exe"), QStringLiteral("Audacity"), QStringLiteral("Someone Else"), app));
        QVERIFY(!WellKnownApps::windowsSignatureMatches(QStringLiteral("Audacity.exe"), QStringLiteral("Unrelated"), {}, app));
    }

    void anApplicationWithVersionInfoRecordedStillRejectsAnotherPublisherEvenWithTheRightExeName()
    {
        const WellKnownApp &lmms = *WellKnownApps::find(QStringLiteral("lmms"));
        QVERIFY(WellKnownApps::windowsSignatureMatches(QStringLiteral("lmms.exe"), QStringLiteral("LMMS"), QStringLiteral("LMMS Developers"), lmms));
        QVERIFY(!WellKnownApps::windowsSignatureMatches(QStringLiteral("lmms.exe"), QStringLiteral("LMMS"), QStringLiteral("Some Other Vendor"), lmms));
    }

    void anExeNameThatOnlyLooksLikeOneOfThemIsNotAccepted()
    {
        QVERIFY(!WellKnownApps::windowsSignatureMatches(QStringLiteral("Ardour2.exe"), {}, {}, *WellKnownApps::find(QStringLiteral("ardour"))));
        QVERIFY(!WellKnownApps::windowsSignatureMatches(QStringLiteral("MuseScore.exe"), {}, {}, *WellKnownApps::find(QStringLiteral("musescore"))));
        QVERIFY(!WellKnownApps::windowsSignatureMatches(QStringLiteral("Audacity.exe"), {}, {}, *WellKnownApps::find(QStringLiteral("kdenlive"))));
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
