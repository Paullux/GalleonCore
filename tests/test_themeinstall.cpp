// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Required.h"

#include <PagesConfig.h>
#include <ThemeInstallPlan.h>
#include <ThemeInstaller.h>
#include <ThemePackage.h>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTest>

using namespace galleond;

namespace {

QByteArray pngHeader()
{
    QByteArray bytes("\x89PNG\r\n\x1a\n", 8);
    bytes.append(QByteArray("\x00\x00\x00\x0d", 4));
    bytes.append("IHDR");
    bytes.append(QByteArray("\x00\x00\x00\x70\x00\x00\x00\x70\x08\x06\x00\x00\x00\x00\x00\x00\x00", 17));
    return bytes;
}

QByteArray jpegHeader()
{
    QByteArray bytes("\xFF\xD8", 2);
    bytes.append(QByteArray("\xFF\xC0\x00\x11\x08\x01\x80\x02\xD0", 9));
    bytes.append(QByteArray("\x03\x01\x22\x00\x02\x11\x01\x03\x11\x01", 10));
    return bytes;
}

QJsonObject manifest()
{
    return QJsonObject{
        {"formatVersion", 1}, {"id", "discord"}, {"name", "Discord"}, {"author", "A. Person"}, {"license", "CC-BY-SA-4.0"}, {"color", "#5865F2"},
        {"keys",
         QJsonArray{
             QJsonObject{{"key", 0}, {"label", "Mute"}, {"icon", "mute.png"}, {"action", QJsonObject{{"type", "key"}, {"key", "kp1"}}}},
             QJsonObject{{"key", 1},
                         {"label", "Discord"},
                         {"icon", "discord"},
                         {"action",
                          QJsonObject{{"type", "app"},
                                      {"desktop", "discord.desktop"},
                                      {"alternatives", QJsonArray{"com.discordapp.Discord.desktop", "discord_discord.desktop"}}}}},
             QJsonObject{{"key", 2}, {"label", "Here"}, {"action", QJsonObject{{"type", "page"}, {"page", "discord"}}}},
         }},
        {"dials", QJsonObject{{"0", QJsonObject{{"turn", QJsonObject{{"type", "media"}, {"command", "volume"}, {"step", 7}}}}}}},
    };
}

ThemePackage package(bool wallpaper = false, const QJsonObject &base = manifest())
{
    std::vector<ZipEntry> entries{{QStringLiteral("theme.json"), QJsonDocument(base).toJson(QJsonDocument::Compact)},
                                  {QStringLiteral("icons/mute.png"), pngHeader()}};
    if (wallpaper) {
        entries.push_back({QStringLiteral("wallpapers/a.jpg"), jpegHeader()});
        entries.push_back({QStringLiteral("wallpapers/b.jpg"), jpegHeader()});
    }
    const ThemeParseResult parsed = parseThemeEntries(entries);
    if (!parsed.package)
        qFatal("the fixture theme is not valid: %s", qPrintable(parsed.errors.join(QLatin1Char(';'))));
    return *parsed.package;
}

ThemeInstallContext context()
{
    ThemeInstallContext c;
    c.existingPages = {QStringLiteral("main")};
    c.existingThemeNames = {QStringLiteral("Music")};
    c.existingThemeCount = 1;
    c.resolveDesktop = [](const QString &) { return QString(); };
    return c;
}

QString keyIcon(const ThemeInstallPlan &plan, int index)
{
    return plan.pageObject.value(QStringLiteral("keys")).toArray().at(index).toObject().value(QStringLiteral("icon")).toString();
}

QString keyDesktop(const ThemeInstallPlan &plan, int index)
{
    return plan.pageObject.value(QStringLiteral("keys")).toArray().at(index).toObject().value(QStringLiteral("action")).toObject().value(QStringLiteral("desktop")).toString();
}

bool writeThemeFile(const QString &path, const ThemePackage &p, bool wallpaper)
{
    std::vector<ZipEntry> entries{{QStringLiteral("theme.json"), QJsonDocument(manifest()).toJson(QJsonDocument::Compact)}};
    for (const ZipEntry &icon : p.icons)
        entries.push_back({QStringLiteral("icons/") + icon.name, icon.data});
    if (wallpaper)
        entries.push_back({QStringLiteral("wallpapers/a.jpg"), jpegHeader()});
    const auto zip = writeStoredZip(entries);
    QFile out(path);
    return zip.has_value() && out.open(QIODevice::WriteOnly) && out.write(required(zip)) == required(zip).size();
}

} // namespace

class TestThemeInstall : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void aPictureIsRenamedWithThePageSoItCannotReplaceAnother()
    {
        const ThemeInstallPlan plan = planThemeInstall(package(), context());
        QVERIFY(plan.ok());
        QCOMPARE(plan.page, QStringLiteral("discord"));
        QCOMPARE(keyIcon(plan, 0), QStringLiteral("discord-mute.png"));
        QCOMPARE(plan.iconFiles.size(), std::size_t{1});
        QCOMPARE(plan.iconFiles.front().name, QStringLiteral("discord-mute.png"));
    }

    void anIconThemeNameIsLeftAlone()
    {
        QCOMPARE(keyIcon(planThemeInstall(package(), context()), 1), QStringLiteral("discord"));
    }

    void aPageNameThatIsTakenGetsANumberAndThePicturesFollow()
    {
        ThemeInstallContext c = context();
        c.existingPages.push_back(QStringLiteral("discord"));
        const ThemeInstallPlan plan = planThemeInstall(package(), c);
        QCOMPARE(plan.page, QStringLiteral("discord-2"));
        QCOMPARE(keyIcon(plan, 0), QStringLiteral("discord-2-mute.png"));
    }

    void aPictureFileThatAlreadyExistsIsNotReplaced()
    {
        ThemeInstallContext c = context();
        c.existingIconFiles = {QStringLiteral("discord-mute.png")};
        const ThemeInstallPlan plan = planThemeInstall(package(), c);
        QCOMPARE(keyIcon(plan, 0), QStringLiteral("discord-mute-2.png"));
        QVERIFY(!c.existingIconFiles.contains(plan.iconFiles.front().name));
    }

    void aPictureFileThatDiffersOnlyByCaseIsNotReplaced()
    {
        ThemeInstallContext c = context();
        c.existingIconFiles = {QStringLiteral("Discord-Mute.PNG")}; // the same file on a Windows volume
        const ThemeInstallPlan plan = planThemeInstall(package(), c);
        QCOMPARE(plan.iconFiles.front().name, QStringLiteral("discord-mute-2.png"));
    }

    void aWallpaperFolderThatDiffersOnlyByCaseIsNotReused()
    {
        ThemeInstallContext c = context();
        c.existingWallpaperStyles = {QStringLiteral("DISCORD")};
        QCOMPARE(planThemeInstall(package(true), c).wallpaperStyle, QStringLiteral("discord-2"));
    }

    void aPathTakenByAnEntryThePlanCannotSeeIsNotReplaced()
    {
        // A dangling link is not listed as a file, so the plan picks its name; creating the picture must still fail.
        const QTemporaryDir dir;
        const QString themeFile = dir.filePath(QStringLiteral("d.gctheme"));
        QVERIFY(writeThemeFile(themeFile, package(), false));
        const QString link = dir.filePath(QStringLiteral("discord-mute.png"));
        if (!QFile::link(dir.filePath(QStringLiteral("nowhere")), link))
            QSKIP("this system cannot make a link");
        const QString pagesFile = dir.filePath(QStringLiteral("pages.json"));
        const ThemeInstallResult result = installTheme({themeFile, pagesFile, dir.filePath(QStringLiteral("wall")), false});
        QVERIFY(!result.ok);
        QVERIFY(!QFile::exists(dir.filePath(QStringLiteral("nowhere")))); // nothing was written through the link
        QVERIFY(QFileInfo(link).isSymLink());                              // and the entry is still there
    }

    void aThemeNameThatIsTakenGetsANumber()
    {
        ThemeInstallContext c = context();
        c.existingThemeNames.push_back(QStringLiteral("Discord"));
        QCOMPARE(planThemeInstall(package(), c).themeName, QStringLiteral("Discord 2"));
    }

    void anOwnPageActionFollowsTheNewPageName()
    {
        ThemeInstallContext c = context();
        c.existingPages.push_back(QStringLiteral("discord"));
        const ThemeInstallPlan plan = planThemeInstall(package(), c);
        const QJsonObject action = plan.pageObject.value(QStringLiteral("keys")).toArray().at(2).toObject().value(QStringLiteral("action")).toObject();
        QCOMPARE(action.value(QStringLiteral("page")).toString(), QStringLiteral("discord-2"));
    }

    void aDialActionOpeningTheOwnPageFollowsTheNewPageName()
    {
        QJsonObject m = manifest();
        m["dials"] = QJsonObject{{"0", QJsonObject{{"press", QJsonObject{{"type", "page"}, {"page", "discord"}}}}}};
        ThemeInstallContext c = context();
        c.applyDials = true;
        c.existingPages.push_back(QStringLiteral("discord"));
        const ThemeInstallPlan plan = planThemeInstall(package(false, m), c);
        const QJsonObject press = plan.dials.value("0").toObject().value("press").toObject();
        QCOMPARE(press.value("page").toString(), QStringLiteral("discord-2"));
    }

    void theInstalledAlternativeIsChosen()
    {
        ThemeInstallContext c = context();
        c.resolveDesktop = [](const QString &id) { return id == QLatin1String("com.discordapp.Discord.desktop") ? id : QString(); };
        const ThemeInstallPlan plan = planThemeInstall(package(), c);
        QCOMPARE(keyDesktop(plan, 1), QStringLiteral("com.discordapp.Discord.desktop"));
        QVERIFY(plan.warnings.isEmpty());
    }

    void theFirstNameWinsWhenSeveralAreInstalled()
    {
        ThemeInstallContext c = context();
        c.resolveDesktop = [](const QString &id) { return id; };
        QCOMPARE(keyDesktop(planThemeInstall(package(), c), 1), QStringLiteral("discord.desktop"));
    }

    void anApplicationThatIsNowhereGivesAWarningNotAnError()
    {
        const ThemeInstallPlan plan = planThemeInstall(package(), context());
        QVERIFY(plan.ok());
        QCOMPARE(keyDesktop(plan, 1), QStringLiteral("discord.desktop"));
        QCOMPARE(plan.warnings.size(), 1);
    }

    void aWellKnownAppIsStoredAsWhatThisMachineActuallyHas()
    {
        QJsonObject fixture = manifest();
        QJsonArray keys = fixture.value(QStringLiteral("keys")).toArray();
        QJsonObject discordKey = keys.at(1).toObject();
        discordKey.insert(QStringLiteral("action"), QJsonObject{{QStringLiteral("type"), QStringLiteral("app")}, {QStringLiteral("desktop"), QStringLiteral("discord")}});
        keys[1] = discordKey;
        fixture.insert(QStringLiteral("keys"), keys);
        const std::vector<ZipEntry> entries{{QStringLiteral("theme.json"), QJsonDocument(fixture).toJson(QJsonDocument::Compact)},
                                            {QStringLiteral("icons/mute.png"), pngHeader()}};
        const ThemeParseResult parsed = parseThemeEntries(entries);
        if (!parsed.package)
            qFatal("the fixture theme is not valid: %s", qPrintable(parsed.errors.join(QLatin1Char(';'))));

        ThemeInstallContext c = context();
        c.resolveDesktop = [](const QString &id) {
            return id == QLatin1String("discord") ? QStringLiteral("C:/Users/test/Discord.lnk") : QString();
        };
        const ThemeInstallPlan plan = planThemeInstall(*parsed.package, c);
        QVERIFY(plan.ok());
        QCOMPARE(keyDesktop(plan, 1), QStringLiteral("C:/Users/test/Discord.lnk"));
        QVERIFY(plan.warnings.isEmpty());
    }

    void theDialsAreCopiedOnlyWhenAsked()
    {
        QVERIFY(planThemeInstall(package(), context()).dials.isEmpty());
        ThemeInstallContext c = context();
        c.applyDials = true;
        QVERIFY(!planThemeInstall(package(), c).dials.isEmpty());
    }

    void noThemeIsAddedPastTheLimit()
    {
        ThemeInstallContext c = context();
        c.existingThemeCount = MaxThemes;
        const ThemeInstallPlan plan = planThemeInstall(package(), c);
        QVERIFY(!plan.ok());
    }

    void wallpapersGoInAFolderNamedAfterThePage()
    {
        const ThemeInstallPlan plan = planThemeInstall(package(true), context());
        QCOMPARE(plan.wallpaperStyle, QStringLiteral("discord"));
        QCOMPARE(plan.wallpaperFiles.size(), std::size_t{2});
        QCOMPARE(plan.wallpaperFiles.front().name, QStringLiteral("discord-01.jpg"));
    }

    void aThemeInstalledIntoNothingStartsFromTheBuiltInPages()
    {
        const QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString themeFile = dir.filePath(QStringLiteral("d.gctheme"));
        QVERIFY(writeThemeFile(themeFile, package(), true));
        const QString pagesFile = dir.filePath(QStringLiteral("cfg/pages.json"));
        const ThemeInstallResult result = installTheme({themeFile, pagesFile, dir.filePath(QStringLiteral("wall")), false});
        QVERIFY2(result.ok, qPrintable(result.errors.join(QLatin1Char(';'))));
        QCOMPARE(result.page, QStringLiteral("discord"));

        QFile in(pagesFile);
        QVERIFY(in.open(QIODevice::ReadOnly));
        const PagesParseResult parsed = parsePagesConfig(in.readAll());
        QVERIFY2(parsed.config.has_value(), qPrintable(parsed.errors.join(QLatin1Char(';'))));
        const PagesConfig &config = required(parsed.config);
        QVERIFY(config.pages.contains(QStringLiteral("discord")));
        QCOMPARE(config.themes.back().name, QStringLiteral("Discord"));
        QCOMPARE(result.themeIndex, static_cast<int>(config.themes.size()) - 1);
        QVERIFY(QFile::exists(dir.filePath(QStringLiteral("cfg/discord-mute.png"))));
        QVERIFY(QFile::exists(dir.filePath(QStringLiteral("wall/discord/discord-01.jpg"))));
        // The first wallpaper of the theme becomes the wallpaper of its tab.
        QCOMPARE(config.themes.back().wallpaper, QStringLiteral("discord/discord-01.jpg"));
    }

    void aThemeWithoutWallpaperLeavesItsTabOnTheUsersOwn()
    {
        const QTemporaryDir dir;
        const QString themeFile = dir.filePath(QStringLiteral("d.gctheme"));
        QVERIFY(writeThemeFile(themeFile, package(), false));
        const QString pagesFile = dir.filePath(QStringLiteral("pages.json"));
        QVERIFY(installTheme({themeFile, pagesFile, dir.filePath(QStringLiteral("wall")), false}).ok);
        QFile in(pagesFile);
        QVERIFY(in.open(QIODevice::ReadOnly));
        QVERIFY(required(parsePagesConfig(in.readAll()).config).themes.back().wallpaper.isEmpty());
    }

    void thePlanNamesTheWallpaperOfTheTab()
    {
        const ThemeInstallPlan plan = planThemeInstall(package(true), context());
        QCOMPARE(plan.wallpaper, QStringLiteral("discord/discord-01.jpg"));
        QVERIFY(planThemeInstall(package(false), context()).wallpaper.isEmpty());
    }

    void installingTwiceMakesTwoPagesAndKeepsTheFirstPictures()
    {
        const QTemporaryDir dir;
        const QString themeFile = dir.filePath(QStringLiteral("d.gctheme"));
        QVERIFY(writeThemeFile(themeFile, package(), false));
        const QString pagesFile = dir.filePath(QStringLiteral("pages.json"));
        const ThemeInstallOptions options{themeFile, pagesFile, dir.filePath(QStringLiteral("wall")), false};
        QVERIFY(installTheme(options).ok);
        const ThemeInstallResult second = installTheme(options);
        QVERIFY2(second.ok, qPrintable(second.errors.join(QLatin1Char(';'))));
        QCOMPARE(second.page, QStringLiteral("discord-2"));
        QCOMPARE(second.themeName, QStringLiteral("Discord 2"));
        QVERIFY(QFile::exists(dir.filePath(QStringLiteral("discord-mute.png"))));
        QVERIFY(QFile::exists(dir.filePath(QStringLiteral("discord-2-mute.png"))));
    }

    void aFailedInstallLeavesNoPictureBehind()
    {
        const QTemporaryDir dir;
        const QString themeFile = dir.filePath(QStringLiteral("d.gctheme"));
        QVERIFY(writeThemeFile(themeFile, package(), true));
        const QString pagesFile = dir.filePath(QStringLiteral("pages.json"));
        QFile broken(pagesFile);
        QVERIFY(broken.open(QIODevice::WriteOnly));
        broken.write("{ this is not json");
        broken.close();
        const ThemeInstallResult result = installTheme({themeFile, pagesFile, dir.filePath(QStringLiteral("wall")), false});
        QVERIFY(!result.ok);
        QVERIFY(!QFile::exists(dir.filePath(QStringLiteral("discord-mute.png"))));
        QVERIFY(!QDir(dir.filePath(QStringLiteral("wall/discord"))).exists());
        QFile after(pagesFile);
        QVERIFY(after.open(QIODevice::ReadOnly));
        QCOMPARE(after.readAll(), QByteArray("{ this is not json"));
    }

    void aFileThatIsNotAThemeChangesNothing()
    {
        const QTemporaryDir dir;
        const QString themeFile = dir.filePath(QStringLiteral("x.gctheme"));
        QFile out(themeFile);
        QVERIFY(out.open(QIODevice::WriteOnly));
        out.write("not a zip");
        out.close();
        const QString pagesFile = dir.filePath(QStringLiteral("pages.json"));
        const ThemeInstallResult result = installTheme({themeFile, pagesFile, dir.filePath(QStringLiteral("wall")), false});
        QVERIFY(!result.ok);
        QVERIFY(!QFile::exists(pagesFile));
        QCOMPARE(QDir(dir.path()).entryList(QDir::Files).size(), 1);
    }

    void aMissingFileIsRefused()
    {
        const QTemporaryDir dir;
        QVERIFY(!installTheme({dir.filePath(QStringLiteral("none.gctheme")), dir.filePath(QStringLiteral("pages.json")), dir.path(), false}).ok);
    }
};

QTEST_MAIN(TestThemeInstall)
#include "test_themeinstall.moc"
