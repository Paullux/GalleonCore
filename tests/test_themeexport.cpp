// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Required.h"

#include <PagesConfig.h>
#include <ThemeExporter.h>
#include <ThemeInstaller.h>
#include <ThemePackage.h>

#include <QBuffer>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QImageReader>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTest>

using namespace galleond;

namespace {

bool savePicture(const QString &path, int width, int height, const char *format)
{
    QImage image(width, height, QImage::Format_RGB32);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x)
            image.setPixel(x, y, qRgb((x * 7) & 0xFF, (y * 5) & 0xFF, ((x + y) * 3) & 0xFF)); // noise-like: JPEG quality shows
    }
    return image.save(path, format);
}

bool writePages(const QString &path, const QJsonObject &page, const QJsonObject &extra = {})
{
    QJsonObject root{{"version", 1},
                     {"home", "mine"},
                     {"pages", QJsonObject{{"mine", page}}},
                     {"themes", QJsonArray{QJsonObject{{"name", "Mine"}, {"page", "mine"}, {"color", "#336699"}}}}};
    for (auto it = extra.begin(); it != extra.end(); ++it)
        root.insert(it.key(), it.value());
    QFile out(path);
    return out.open(QIODevice::WriteOnly) && out.write(QJsonDocument(root).toJson()) > 0;
}

QJsonObject simpleKeys()
{
    return QJsonObject{{"keys",
                        QJsonArray{
                            QJsonObject{{"key", 0}, {"label", "Mute"}, {"icon", "mine-mute.png"}, {"action", QJsonObject{{"type", "key"}, {"key", "kp1"}}}},
                            QJsonObject{{"key", 1}, {"label", "Next"}, {"icon", "media-skip-forward"}, {"action", QJsonObject{{"type", "media"}, {"command", "next"}}}},
                        }}};
}

ThemeExportOptions options(const QString &dir)
{
    ThemeExportOptions o;
    o.pagesFile = QDir(dir).filePath(QStringLiteral("pages.json"));
    o.author = QStringLiteral("A. Person");
    o.license = QStringLiteral("CC-BY-SA-4.0");
    return o;
}

QString joined(const QStringList &list)
{
    return list.join(QLatin1Char(';'));
}

QSize sizeOf(const QByteArray &bytes)
{
    QBuffer buffer;
    buffer.setData(bytes);
    buffer.open(QIODevice::ReadOnly);
    return QImageReader(&buffer).size();
}

} // namespace

class TestThemeExport : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void anExportedThemeIsAcceptedByTheImporterAndDropsThePagePrefix()
    {
        const QTemporaryDir dir;
        QVERIFY(writePages(dir.filePath(QStringLiteral("pages.json")), simpleKeys()));
        QVERIFY(savePicture(dir.filePath(QStringLiteral("mine-mute.png")), 64, 64, "PNG"));
        const ThemeExportResult result = exportTheme(options(dir.path()));
        QVERIFY2(result.file.has_value(), qPrintable(joined(result.errors)));
        const ThemeParseResult parsed = parseThemePackage(required(result.file));
        QVERIFY2(parsed.package.has_value(), qPrintable(joined(parsed.errors)));
        const ThemePackage &package = required(parsed.package);
        QCOMPARE(package.meta.id, QStringLiteral("mine"));
        QCOMPARE(package.meta.license, QStringLiteral("CC-BY-SA-4.0"));
        QCOMPARE(package.config.themes.front().rgb, 0x336699U);
        QCOMPARE(package.icons.size(), std::size_t{1});
        QCOMPARE(package.icons.front().name, QStringLiteral("mute.png"));
    }

    void aLargeKeyPictureIsShrunkToTheKeySize()
    {
        const QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("big.png"));
        QVERIFY(savePicture(path, 400, 300, "PNG"));
        QFile in(path);
        QVERIFY(in.open(QIODevice::ReadOnly));
        const auto out = reencodeIcon(in.readAll());
        QVERIFY(out.has_value());
        QVERIFY(sizeOf(required(out)).width() <= ThemeIconSize);
        QVERIFY(sizeOf(required(out)).height() <= ThemeIconSize);
    }

    void aWallpaperIsReencodedAsAJpegNoLargerThanTheScreen()
    {
        const QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("w.png"));
        QVERIFY(savePicture(path, 2000, 1000, "PNG"));
        QFile in(path);
        QVERIFY(in.open(QIODevice::ReadOnly));
        const auto out = reencodeWallpaper(in.readAll(), 85);
        QVERIFY(out.has_value());
        QCOMPARE(required(out).left(2), QByteArray("\xFF\xD8", 2));
        QVERIFY(sizeOf(required(out)).width() <= ThemeWallpaperMaxWidth);
        QVERIFY(sizeOf(required(out)).height() <= ThemeWallpaperMaxHeight);
    }

    void aSmallWallpaperIsNotEnlarged()
    {
        const QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("w.png"));
        QVERIFY(savePicture(path, 300, 200, "PNG"));
        QFile in(path);
        QVERIFY(in.open(QIODevice::ReadOnly));
        QCOMPARE(sizeOf(required(reencodeWallpaper(in.readAll(), 85))), QSize(300, 200));
    }

    void theJpegQualityNeverGoesBelowTheFloor()
    {
        const QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("w.png"));
        QVERIFY(savePicture(path, 600, 380, "PNG"));
        QFile in(path);
        QVERIFY(in.open(QIODevice::ReadOnly));
        const QByteArray source = in.readAll();
        QCOMPARE(reencodeWallpaper(source, 5), reencodeWallpaper(source, MinWallpaperQuality));
        QVERIFY(required(reencodeWallpaper(source, 100)).size() > required(reencodeWallpaper(source, MinWallpaperQuality)).size());
    }

    void somethingThatIsNotAnImageIsRefused()
    {
        QVERIFY(!reencodeIcon(QByteArray("not an image")).has_value());
        QVERIFY(!reencodeWallpaper(QByteArray(), 85).has_value());
    }

    void wallpapersAreIncludedAndCheckedByTheImporter()
    {
        const QTemporaryDir dir;
        QVERIFY(writePages(dir.filePath(QStringLiteral("pages.json")), simpleKeys()));
        QVERIFY(savePicture(dir.filePath(QStringLiteral("mine-mute.png")), 64, 64, "PNG"));
        QVERIFY(savePicture(dir.filePath(QStringLiteral("bg.png")), 1500, 900, "PNG"));
        ThemeExportOptions o = options(dir.path());
        o.wallpaperFiles = {dir.filePath(QStringLiteral("bg.png"))};
        o.credits = QStringLiteral("Pictures by A. Person\n");
        const ThemeExportResult result = exportTheme(o);
        QVERIFY2(result.file.has_value(), qPrintable(joined(result.errors)));
        const ThemeParseResult parsed = parseThemePackage(required(result.file));
        QVERIFY2(parsed.package.has_value(), qPrintable(joined(parsed.errors)));
        QCOMPARE(required(parsed.package).wallpapers.size(), std::size_t{1});
        QVERIFY(required(parsed.package).meta.credits.contains(QLatin1String("A. Person")));
    }

    void anApplicationGivenByAPathIsRefusedAndTheKeyIsNamed()
    {
        const QTemporaryDir dir;
        const QJsonObject page{{"keys", QJsonArray{QJsonObject{{"key", 4}, {"label", "X"}, {"action", QJsonObject{{"type", "app"}, {"desktop", "/home/me/x.desktop"}}}}}}};
        QVERIFY(writePages(dir.filePath(QStringLiteral("pages.json")), page));
        const ThemeExportResult result = exportTheme(options(dir.path()));
        QVERIFY(!result.file.has_value());
        QVERIFY2(joined(result.errors).contains(QLatin1String("keys[4]")), qPrintable(joined(result.errors)));
    }

    void aMissingPictureStopsTheExport()
    {
        const QTemporaryDir dir;
        QVERIFY(writePages(dir.filePath(QStringLiteral("pages.json")), simpleKeys()));
        const ThemeExportResult result = exportTheme(options(dir.path()));
        QVERIFY(!result.file.has_value());
        QVERIFY(joined(result.errors).contains(QLatin1String("mine-mute.png")));
    }

    void aPictureThatIsASymbolicLinkIsRefused()
    {
        const QTemporaryDir dir;
        QVERIFY(writePages(dir.filePath(QStringLiteral("pages.json")), simpleKeys()));
        QVERIFY(savePicture(dir.filePath(QStringLiteral("real.png")), 32, 32, "PNG"));
        QVERIFY(QFile::link(dir.filePath(QStringLiteral("real.png")), dir.filePath(QStringLiteral("mine-mute.png"))));
        QVERIFY(!exportTheme(options(dir.path())).file.has_value());
    }

    void aLicenseIsRequired()
    {
        const QTemporaryDir dir;
        QVERIFY(writePages(dir.filePath(QStringLiteral("pages.json")), QJsonObject{{"keys", QJsonArray{}}}));
        ThemeExportOptions o = options(dir.path());
        o.license.clear();
        const ThemeExportResult result = exportTheme(o);
        QVERIFY(!result.file.has_value());
        QVERIFY(joined(result.errors).contains(QLatin1String("license")));
    }

    void anUnknownThemeIsRefused()
    {
        const QTemporaryDir dir;
        QVERIFY(writePages(dir.filePath(QStringLiteral("pages.json")), QJsonObject{{"keys", QJsonArray{}}}));
        ThemeExportOptions o = options(dir.path());
        o.themeIndex = 3;
        QVERIFY(!exportTheme(o).file.has_value());
        o.themeIndex = -1;
        QVERIFY(!exportTheme(o).file.has_value());
    }

    void tooManyWallpapersAreRefused()
    {
        const QTemporaryDir dir;
        QVERIFY(writePages(dir.filePath(QStringLiteral("pages.json")), QJsonObject{{"keys", QJsonArray{}}}));
        ThemeExportOptions o = options(dir.path());
        o.wallpaperFiles = QStringList(MaxThemeWallpapers + 1, QStringLiteral("x.jpg"));
        QVERIFY(!exportTheme(o).file.has_value());
    }

    void autoFillSettingsAreLeftOutWithAWarning()
    {
        const QTemporaryDir dir;
        const QJsonObject page{{"keys", QJsonArray{}}, {"steam", QJsonObject{{"max", 3}}}};
        QVERIFY(writePages(dir.filePath(QStringLiteral("pages.json")), page));
        const ThemeExportResult result = exportTheme(options(dir.path()));
        QVERIFY2(result.file.has_value(), qPrintable(joined(result.errors)));
        QVERIFY(joined(result.warnings).contains(QLatin1String("steam")));
    }

    void theDialsAreLeftOutUnlessAsked()
    {
        const QTemporaryDir dir;
        const QJsonObject dials{{"0", QJsonObject{{"turn", QJsonObject{{"type", "media"}, {"command", "volume"}, {"step", 3}}}}}};
        QVERIFY(writePages(dir.filePath(QStringLiteral("pages.json")), QJsonObject{{"keys", QJsonArray{}}}, QJsonObject{{"dials", dials}}));
        ThemeExportOptions o = options(dir.path());
        auto without = parseThemePackage(required(exportTheme(o).file));
        QVERIFY(required(without.package).dialsJson.isEmpty());
        o.includeDials = true;
        auto with = parseThemePackage(required(exportTheme(o).file));
        QVERIFY(!required(with.package).dialsJson.isEmpty());
    }

    void aThemeExportedThenInstalledGivesTheSameKeys()
    {
        const QTemporaryDir source;
        QVERIFY(writePages(source.filePath(QStringLiteral("pages.json")), simpleKeys()));
        QVERIFY(savePicture(source.filePath(QStringLiteral("mine-mute.png")), 64, 64, "PNG"));
        const ThemeExportResult exported = exportTheme(options(source.path()));
        QVERIFY2(exported.file.has_value(), qPrintable(joined(exported.errors)));

        const QTemporaryDir target;
        const QString themeFile = target.filePath(QStringLiteral("mine.gc.theme"));
        QFile out(themeFile);
        QVERIFY(out.open(QIODevice::WriteOnly));
        out.write(required(exported.file));
        out.close();
        const QString pagesFile = target.filePath(QStringLiteral("pages.json"));
        const ThemeInstallResult installed = installTheme({themeFile, pagesFile, target.filePath(QStringLiteral("wall")), false});
        QVERIFY2(installed.ok, qPrintable(joined(installed.errors)));

        QFile in(pagesFile);
        QVERIFY(in.open(QIODevice::ReadOnly));
        const PagesConfig config = required(parsePagesConfig(in.readAll()).config);
        const Page &page = config.pages.value(QStringLiteral("mine"));
        QCOMPARE(required(page.keys.at(0)).label, QStringLiteral("Mute"));
        QCOMPARE(required(page.keys.at(0)).action.keyName, QStringLiteral("kp1"));
        QCOMPARE(required(page.keys.at(1)).action.command, MediaCommand::Next);
        QVERIFY(QFile::exists(target.filePath(QStringLiteral("mine-mute.png"))));
    }
};

QTEST_MAIN(TestThemeExport)
#include "test_themeexport.moc"
