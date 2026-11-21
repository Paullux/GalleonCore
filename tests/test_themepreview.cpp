// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

// Bosun's open theme tab puts the module on that theme and gives it back: what is remembered and what is given back.
#include "ThemePreview.h"

#include <QTest>

using namespace galleond;

namespace {
// A function, not a constant: a QStringList at namespace scope may throw while the program starts (cert-err58-cpp).
QStringList themePages()
{
    return {"a", "b", "c", "numpad"};
}
}

class TestThemePreview : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void theModuleGoesBackToTheThemeItShowedBeforeTheFirstTab()
    {
        ThemePreview preview;
        QVERIFY(preview.begin("a", "b", false));
        QVERIFY(preview.begin("b", "c", false)); // the next tab does not replace the way back
        const auto revert = preview.end("c", false, themePages());
        QCOMPARE(revert.kind, ThemePreview::Kind::Show);
        QCOMPARE(revert.page, QString("a"));
        QVERIFY(!preview.active());
    }

    void aTabIsRefusedWhileAGameRuns()
    {
        ThemePreview preview;
        QVERIFY(!preview.begin("a", "b", true));
        QVERIFY(!preview.active());
        QCOMPARE(preview.end("a", true, themePages()).kind, ThemePreview::Kind::Nothing);
    }

    void anEmptyPageIsRefused()
    {
        ThemePreview preview;
        QVERIFY(!preview.begin("a", QString(), false));
    }

    void nothingIsGivenBackWhenThePreviewNeverStarted()
    {
        ThemePreview preview;
        QCOMPARE(preview.end("a", false, themePages()).kind, ThemePreview::Kind::Nothing);
    }

    void aModuleChangedSinceTheTabWasShownIsLeftAlone()
    {
        ThemePreview preview;
        QVERIFY(preview.begin("a", "b", false));
        QCOMPARE(preview.end("c", false, themePages()).kind, ThemePreview::Kind::Nothing); // a dial or a player moved it to c
        QVERIFY(!preview.active());
    }

    void aThemeThatIsGoneIsReplacedByTheNumpad()
    {
        ThemePreview preview;
        QVERIFY(preview.begin("_galleon_gaming", "b", false)); // a game's page does not outlive the game
        const auto revert = preview.end("b", false, themePages());
        QCOMPARE(revert.kind, ThemePreview::Kind::Show);
        QCOMPARE(revert.page, QString("numpad"));
    }

    void withoutANumpadThemeAGoneThemeLeavesTheModuleAsItIs()
    {
        ThemePreview preview;
        QVERIFY(preview.begin("gone", "b", false));
        QCOMPARE(preview.end("b", false, QStringList{"a", "b"}).kind, ThemePreview::Kind::Nothing);
    }

    void whenAGameRunsAtTheEndTheWayBackWaitsForTheGameToEnd()
    {
        ThemePreview preview;
        QVERIFY(preview.begin("a", "b", false));
        const auto revert = preview.end("b", true, themePages());
        QCOMPARE(revert.kind, ThemePreview::Kind::AfterGame);
        QCOMPARE(revert.page, QString("a"));
    }

    void aThemeShownForGoodForgetsThePreview()
    {
        ThemePreview preview;
        QVERIFY(preview.begin("a", "b", false));
        preview.clear();
        QCOMPARE(preview.end("b", false, themePages()).kind, ThemePreview::Kind::Nothing);
    }

    void aSecondPreviewAfterTheEndRemembersTheNewStart()
    {
        ThemePreview preview;
        QVERIFY(preview.begin("a", "b", false));
        QVERIFY(preview.end("b", false, themePages()).kind == ThemePreview::Kind::Show);
        QVERIFY(preview.begin("c", "b", false));
        QCOMPARE(preview.end("b", false, themePages()).page, QString("c"));
    }
};

QTEST_APPLESS_MAIN(TestThemePreview)
#include "test_themepreview.moc"
