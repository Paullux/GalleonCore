// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Required.h"

#include <ThemePackage.h>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDir>
#include <QFile>
#include <QTest>

using namespace galleond;

namespace {

void putBe32(QByteArray &bytes, quint32 value)
{
    for (int shift = 24; shift >= 0; shift -= 8)
        bytes.append(static_cast<char>((value >> shift) & 0xFFU));
}

// The first bytes of a PNG: signature and an IHDR chunk. Enough for what the reader looks at.
QByteArray pngHeader(quint32 width, quint32 height)
{
    QByteArray bytes("\x89PNG\r\n\x1a\n", 8);
    putBe32(bytes, 13);
    bytes.append("IHDR");
    putBe32(bytes, width);
    putBe32(bytes, height);
    bytes.append(QByteArray("\x08\x06\x00\x00\x00", 5));
    putBe32(bytes, 0); // checksum, not looked at
    return bytes;
}

// A JPEG that holds only a frame header.
QByteArray jpegHeader(int width, int height)
{
    QByteArray bytes("\xFF\xD8", 2);
    bytes.append(QByteArray("\xFF\xC0\x00\x11\x08", 5));
    bytes.append(static_cast<char>(height >> 8));
    bytes.append(static_cast<char>(height & 0xFF));
    bytes.append(static_cast<char>(width >> 8));
    bytes.append(static_cast<char>(width & 0xFF));
    bytes.append(QByteArray("\x03\x01\x22\x00\x02\x11\x01\x03\x11\x01", 10));
    return bytes;
}

QJsonObject validManifest()
{
    return QJsonObject{
        {"formatVersion", 1},
        {"id", "discord"},
        {"name", "Discord"},
        {"author", "A. Person"},
        {"license", "CC-BY-SA-4.0"},
        {"description", "Voice keys"},
        {"color", "#5865F2"},
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
             QJsonObject{{"key", 2}, {"label", "Play"}, {"action", QJsonObject{{"type", "media"}, {"command", "play-pause"}}}},
         }},
        {"dials", QJsonObject{{"0", QJsonObject{{"turn", QJsonObject{{"type", "media"}, {"command", "volume"}, {"step", 2}}}}}}},
    };
}

ZipEntry manifestEntry(const QJsonObject &manifest)
{
    return {QStringLiteral("theme.json"), QJsonDocument(manifest).toJson(QJsonDocument::Compact)};
}

std::vector<ZipEntry> validEntries()
{
    return {manifestEntry(validManifest()), {QStringLiteral("icons/mute.png"), pngHeader(112, 112)}};
}

QString joined(const ThemeParseResult &result)
{
    return result.errors.join(QLatin1Char('\n'));
}

} // namespace

class TestThemePackage : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void aValidThemeIsAccepted()
    {
        const ThemeParseResult result = parseThemeEntries(validEntries());
        QVERIFY2(result.package.has_value(), qPrintable(joined(result)));
        const ThemePackage &package = required(result.package);
        QCOMPARE(package.meta.id, QStringLiteral("discord"));
        QCOMPARE(package.meta.license, QStringLiteral("CC-BY-SA-4.0"));
        QCOMPARE(package.config.home, QStringLiteral("discord"));
        QCOMPARE(package.config.themes.size(), std::size_t{1});
        QCOMPARE(package.config.themes.front().rgb, 0x5865F2U);
        QCOMPARE(package.config.pages.size(), 1);
        QCOMPARE(package.icons.size(), std::size_t{1});
    }

    void aThemeSurvivesBeingWrittenAndReadAsAFile()
    {
        const auto file = writeStoredZip(validEntries());
        QVERIFY(file.has_value());
        const ThemeParseResult result = parseThemePackage(required(file));
        QVERIFY2(result.package.has_value(), qPrintable(joined(result)));
    }

    void alternativeApplicationsAreKeptByKey()
    {
        const ThemeParseResult result = parseThemeEntries(validEntries());
        QVERIFY(result.package.has_value());
        QCOMPARE(required(result.package).appAlternatives.value(1),
                 (QStringList{QStringLiteral("com.discordapp.Discord.desktop"), QStringLiteral("discord_discord.desktop")}));
        QVERIFY(!required(result.package).appAlternatives.contains(0));
    }

    void aWallpaperAndCreditsAreAccepted()
    {
        auto entries = validEntries();
        entries.push_back({QStringLiteral("wallpapers/bg.jpg"), jpegHeader(720, 384)});
        entries.push_back({QStringLiteral("credits.txt"), QByteArray("Icons by A. Person, CC-BY-SA-4.0\n")});
        const ThemeParseResult result = parseThemeEntries(entries);
        QVERIFY2(result.package.has_value(), qPrintable(joined(result)));
        QCOMPARE(required(result.package).wallpapers.size(), std::size_t{1});
        QVERIFY(required(result.package).meta.credits.contains(QLatin1String("A. Person")));
    }

    void aThemeWithoutAManifestIsRefused()
    {
        const ThemeParseResult result = parseThemeEntries({{QStringLiteral("icons/mute.png"), pngHeader(1, 1)}});
        QVERIFY(!result.package.has_value());
        QVERIFY(joined(result).contains(QLatin1String("theme.json")));
    }

    void aManifestThatIsNotJsonIsRefused()
    {
        QVERIFY(!parseThemeEntries({{QStringLiteral("theme.json"), QByteArray("{ not json")}}).package.has_value());
        QVERIFY(!parseThemeEntries({{QStringLiteral("theme.json"), QByteArray("[1,2]")}}).package.has_value());
    }

    void anUnknownFormatVersionIsRefused()
    {
        QJsonObject manifest = validManifest();
        manifest["formatVersion"] = 2;
        QVERIFY(joined(parseThemeEntries({manifestEntry(manifest), validEntries().at(1)})).contains(QLatin1String("formatVersion")));
        manifest.remove("formatVersion");
        QVERIFY(!parseThemeEntries({manifestEntry(manifest), validEntries().at(1)}).package.has_value());
    }

    void aMissingLicenseIsRefused()
    {
        QJsonObject manifest = validManifest();
        manifest.remove("license");
        const ThemeParseResult result = parseThemeEntries({manifestEntry(manifest), validEntries().at(1)});
        QVERIFY(!result.package.has_value());
        QVERIFY(joined(result).contains(QLatin1String("license")));
    }

    void aFieldThePackageDoesNotKnowIsRefused()
    {
        QJsonObject manifest = validManifest();
        manifest["shell"] = "rm -rf /";
        QVERIFY(joined(parseThemeEntries({manifestEntry(manifest), validEntries().at(1)})).contains(QLatin1String("shell")));
    }

    void aBadIdentifierIsRefused_data()
    {
        QTest::addColumn<QString>("id");
        QTest::newRow("upper case") << "Discord";
        QTest::newRow("slash") << "a/b";
        QTest::newRow("dots") << "..";
        QTest::newRow("too long") << QString(33, QLatin1Char('a'));
        QTest::newRow("empty") << "";
    }
    void aBadIdentifierIsRefused()
    {
        QFETCH(const QString, id);
        QJsonObject manifest = validManifest();
        manifest["id"] = id;
        QVERIFY(!parseThemeEntries({manifestEntry(manifest), validEntries().at(1)}).package.has_value());
    }

    void actionsThatCouldHarmAreRefused_data()
    {
        QTest::addColumn<QString>("actionJson");
        QTest::addColumn<QString>("expected");
        QTest::newRow("app by path") << R"({"type":"app","desktop":"/home/x/evil.desktop"})" << "desktop file name";
        QTest::newRow("app by an unknown bare word") << R"({"type":"app","desktop":"not-a-known-app"})" << "desktop file name";
        QTest::newRow("another page") << R"({"type":"page","page":"main"})" << "main";
        QTest::newRow("switch theme") << R"({"type":"theme"})" << "switch themes";
        QTest::newRow("unknown type") << R"({"type":"shell","command":"id"})" << "type";
        QTest::newRow("a command field") << R"({"type":"media","command":"next","exec":"id"})" << "exec";
    }
    void actionsThatCouldHarmAreRefused()
    {
        QFETCH(const QString, actionJson);
        QFETCH(const QString, expected);
        QJsonObject manifest = validManifest();
        manifest["keys"] = QJsonArray{
            QJsonObject{{"key", 0}, {"label", "X"}, {"action", QJsonDocument::fromJson(actionJson.toUtf8()).object()}},
        };
        const ThemeParseResult result = parseThemeEntries({manifestEntry(manifest)});
        QVERIFY(!result.package.has_value());
        QVERIFY2(joined(result).contains(expected), qPrintable(joined(result)));
    }

    void anOwnPageActionIsAccepted()
    {
        QJsonObject manifest = validManifest();
        manifest["keys"] = QJsonArray{
            QJsonObject{{"key", 0}, {"label", "Here"}, {"action", QJsonObject{{"type", "page"}, {"page", "discord"}}}},
        };
        QVERIFY(parseThemeEntries({manifestEntry(manifest)}).package.has_value());
    }

    void aPortableAppIdIsAccepted()
    {
        QJsonObject manifest = validManifest();
        manifest["keys"] = QJsonArray{
            QJsonObject{{"key", 0}, {"label", "Discord"}, {"action", QJsonObject{{"type", "app"}, {"desktop", "discord"}}}},
        };
        QVERIFY(parseThemeEntries({manifestEntry(manifest)}).package.has_value());
    }

    void alternativesMustBeDesktopFileNames()
    {
        QJsonObject manifest = validManifest();
        QJsonArray keys = manifest["keys"].toArray();
        QJsonObject key = keys.at(1).toObject();
        QJsonObject action = key["action"].toObject();
        action["alternatives"] = QJsonArray{"/usr/bin/evil"};
        key["action"] = action;
        keys[1] = key;
        manifest["keys"] = keys;
        QVERIFY(!parseThemeEntries({manifestEntry(manifest), validEntries().at(1)}).package.has_value());
    }

    void alternativesOnAnotherKindOfActionAreRefused()
    {
        QJsonObject manifest = validManifest();
        manifest["keys"] = QJsonArray{
            QJsonObject{{"key", 0}, {"label", "X"}, {"action", QJsonObject{{"type", "media"}, {"command", "next"}, {"alternatives", QJsonArray{"a.desktop"}}}}},
        };
        QVERIFY(joined(parseThemeEntries({manifestEntry(manifest)})).contains(QLatin1String("alternatives")));
    }

    void anIconThePackageLacksIsRefused()
    {
        const ThemeParseResult result = parseThemeEntries({manifestEntry(validManifest())});
        QVERIFY(!result.package.has_value());
        QVERIFY(joined(result).contains(QLatin1String("mute.png")));
    }

    void aPictureNoKeyUsesIsRefused()
    {
        auto entries = validEntries();
        entries.push_back({QStringLiteral("icons/spare.png"), pngHeader(8, 8)});
        QVERIFY(joined(parseThemeEntries(entries)).contains(QLatin1String("spare.png")));
    }

    void anIconThatIsNotAPngIsRefused()
    {
        auto entries = validEntries();
        entries[1].data = QByteArray("GIF89a not a png at all, but long enough to pass a length test....");
        QVERIFY(joined(parseThemeEntries(entries)).contains(QLatin1String("not a PNG")));
    }

    void aHugeIconIsRefusedByItsHeaderAlone()
    {
        auto entries = validEntries();
        entries[1].data = pngHeader(60000, 60000);
        QVERIFY(joined(parseThemeEntries(entries)).contains(QLatin1String("pixels")));
    }

    void anIconFileWithAnotherExtensionIsRefused()
    {
        auto entries = validEntries();
        entries.push_back({QStringLiteral("icons/run.svg"), QByteArray("<svg/>")});
        QVERIFY(!parseThemeEntries(entries).package.has_value());
    }

    void aWallpaperMustBeAJpegOfSaneSize()
    {
        auto entries = validEntries();
        entries.push_back({QStringLiteral("wallpapers/bg.jpg"), pngHeader(720, 384)});
        QVERIFY(joined(parseThemeEntries(entries)).contains(QLatin1String("not a JPEG")));
        entries.back().data = jpegHeader(9000, 9000);
        QVERIFY(joined(parseThemeEntries(entries)).contains(QLatin1String("pixels")));
    }

    void tooManyWallpapersAreRefused()
    {
        auto entries = validEntries();
        entries.reserve(MaxThemeWallpapers + 3);
        for (int i = 0; i <= MaxThemeWallpapers; ++i)
            entries.push_back({QStringLiteral("wallpapers/w%1.jpg").arg(i), jpegHeader(720, 384)});
        QVERIFY(!parseThemeEntries(entries).package.has_value());
    }

    void aFileOfAnotherKindIsRefused()
    {
        auto entries = validEntries();
        entries.push_back({QStringLiteral("run.sh"), QByteArray("#!/bin/sh\nid\n")});
        const ThemeParseResult result = parseThemeEntries(entries);
        QVERIFY(!result.package.has_value());
        QVERIFY(joined(result).contains(QLatin1String("run.sh")));
    }

    void creditsWithControlCharactersAreRefused()
    {
        auto entries = validEntries();
        entries.push_back({QStringLiteral("credits.txt"), QByteArray("hello\x1b[31mred")});
        QVERIFY(!parseThemeEntries(entries).package.has_value());
    }

    void aTextFieldWithAControlCharacterIsRefused()
    {
        QJsonObject manifest = validManifest();
        manifest["author"] = QStringLiteral("A.\x1b[2J Person");
        QVERIFY(!parseThemeEntries({manifestEntry(manifest), validEntries().at(1)}).package.has_value());
    }

    void anyPageFeatureNotInTheListIsRefused()
    {
        // The pages file allows a page to be filled with Steam games or menu entries; a theme has fixed keys only.
        QJsonObject manifest = validManifest();
        manifest["steam"] = QJsonObject{{"max", 6}};
        QVERIFY(!parseThemeEntries({manifestEntry(manifest), validEntries().at(1)}).package.has_value());
    }

    // The themes shipped in the repository are checked by the same validator as anyone's.
    void everyThemeShippedWithGalleonCoreIsValid()
    {
        const QDir themes(QString::fromUtf8(GALLEON_SOURCE_THEMES));
        QVERIFY2(themes.exists(), "the themes folder of the source tree is missing");
        const QStringList folders = themes.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
        QVERIFY(!folders.isEmpty());
        for (const QString &folder : folders) {
            const QDir theme(themes.filePath(folder));
            std::vector<ZipEntry> entries;
            for (const QString &name : {QStringLiteral("theme.json"), QStringLiteral("credits.txt")}) {
                QFile file(theme.filePath(name));
                if (file.open(QIODevice::ReadOnly))
                    entries.push_back({name, file.readAll()});
            }
            for (const QString &sub : {QStringLiteral("icons"), QStringLiteral("wallpapers")}) {
                const QStringList files = QDir(theme.filePath(sub)).entryList(QDir::Files);
                for (const QString &name : files) {
                    QFile file(theme.filePath(sub + QLatin1Char('/') + name));
                    QVERIFY(file.open(QIODevice::ReadOnly));
                    entries.push_back({sub + QLatin1Char('/') + name, file.readAll()});
                }
            }
            const ThemeParseResult result = parseThemeEntries(entries);
            QVERIFY2(result.package.has_value(), qPrintable(folder + QLatin1String(": ") + joined(result)));
            QCOMPARE(required(result.package).meta.id, folder); // the folder is named after the theme
            QCOMPARE(required(result.package).meta.license, QStringLiteral("CC-BY-SA-4.0"));
        }
    }

    void imageInspectionReadsTheSizeFromTheHeader()
    {
        const auto png = inspectImage(pngHeader(112, 64));
        QVERIFY(png.has_value());
        QCOMPARE(required(png).width, 112);
        QCOMPARE(required(png).height, 64);
        const auto jpeg = inspectImage(jpegHeader(720, 384));
        QVERIFY(jpeg.has_value());
        QCOMPARE(required(jpeg).width, 720);
        QCOMPARE(required(jpeg).height, 384);
        QVERIFY(!inspectImage(QByteArray("plain text")).has_value());
        QVERIFY(!inspectImage(QByteArray()).has_value());
    }

    void truncatedPicturesAreNotInspectedPastTheirEnd()
    {
        const QByteArray png = pngHeader(10, 10);
        const QByteArray jpeg = jpegHeader(10, 10);
        for (qsizetype length = 0; length < png.size(); ++length)
            QVERIFY(!inspectImage(png.left(length)).has_value());
        for (qsizetype length = 0; length < jpeg.size(); ++length)
            QVERIFY(!inspectImage(jpeg.left(length)).has_value());
    }
};

QTEST_APPLESS_MAIN(TestThemePackage)
#include "test_themepackage.moc"
