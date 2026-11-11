// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ThemeExporter.h"

#include "InitialPages.h"
#include "PagesConfig.h"
#include "ThemePackage.h"
#include "WallpaperCatalog.h"

#include <QBuffer>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QImageReader>
#include <QImageWriter>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <algorithm>

namespace galleond {

namespace {

constexpr int MaxSourceDimension = 8192;
constexpr qint64 MaxSourceBytes = 32LL * 1024 * 1024;

std::optional<QImage> decode(const QByteArray &picture)
{
    if (picture.isEmpty() || picture.size() > MaxSourceBytes)
        return std::nullopt;
    QBuffer buffer;
    buffer.setData(picture);
    if (!buffer.open(QIODevice::ReadOnly))
        return std::nullopt;
    QImageReader reader(&buffer);
    const QSize size = reader.size();
    // The size is known from the header, before anything is decoded.
    if (!size.isValid() || size.width() > MaxSourceDimension || size.height() > MaxSourceDimension)
        return std::nullopt;
    QImage image = reader.read();
    if (image.isNull())
        return std::nullopt;
    return image;
}

std::optional<QByteArray> encode(const QImage &image, const char *format, int quality)
{
    QByteArray out;
    QBuffer buffer(&out);
    if (!buffer.open(QIODevice::WriteOnly))
        return std::nullopt;
    QImageWriter writer(&buffer, format);
    if (quality >= 0)
        writer.setQuality(quality);
    if (!writer.write(image))
        return std::nullopt;
    return out;
}

// Every picture name a key uses that is a file (it has an extension).
QStringList fileIconNames(const QJsonValue &icon)
{
    QStringList names;
    const auto add = [&names](const QString &name) {
        if (name.contains(QLatin1Char('.')) && !names.contains(name))
            names.push_back(name);
    };
    if (icon.isString())
        add(icon.toString());
    else if (icon.isObject())
        for (const auto &value : icon.toObject())
            add(value.toString());
    return names;
}

QJsonValue renamedIcon(const QJsonValue &icon, const QHash<QString, QString> &names)
{
    const auto one = [&names](const QString &name) { return names.value(name, name); };
    if (icon.isString())
        return one(icon.toString());
    if (!icon.isObject())
        return icon;
    QJsonObject states = icon.toObject();
    for (auto it = states.begin(); it != states.end(); ++it)
        it.value() = one(it.value().toString());
    return states;
}

// "discord-mute.png" -> "mute.png" when the page prefix is there and what is left is a name.
QString portableName(const QString &name, const QString &page)
{
    const QString prefix = page + QLatin1Char('-');
    if (name.startsWith(prefix) && name.size() > prefix.size() + 4)
        return name.mid(prefix.size());
    return name;
}

QString colorText(std::uint32_t rgb)
{
    return QStringLiteral("#%1").arg(rgb & 0xFFFFFFU, 6, 16, QLatin1Char('0'));
}

class Exporter
{
public:
    explicit Exporter(const ThemeExportOptions &options) : m_options(options) {}

    ThemeExportResult run()
    {
        if (!load())
            return finish();
        const Theme &theme = m_config.themes.at(static_cast<std::size_t>(m_options.themeIndex));
        const QJsonObject pageObject = m_root.value(QStringLiteral("pages")).toObject().value(theme.page).toObject();
        QJsonArray keys = pageObject.value(QStringLiteral("keys")).toArray();
        for (const QString &field : {QStringLiteral("steam"), QStringLiteral("menu"), QStringLiteral("sort")}) {
            if (pageObject.contains(field))
                m_warnings.push_back(QStringLiteral("the page's \"%1\" setting depends on this computer and is not part of the theme").arg(field));
        }
        collectPictures(keys, theme.page);
        QJsonObject manifest{{QStringLiteral("formatVersion"), ThemeFormatVersion},
                             {QStringLiteral("id"), theme.page},
                             {QStringLiteral("name"), theme.name},
                             {QStringLiteral("author"), m_options.author},
                             {QStringLiteral("license"), m_options.license},
                             {QStringLiteral("color"), colorText(theme.rgb)},
                             {QStringLiteral("keys"), keys}};
        if (!m_options.description.isEmpty())
            manifest.insert(QStringLiteral("description"), m_options.description);
        if (m_options.includeDials)
            manifest.insert(QStringLiteral("dials"), m_root.value(QStringLiteral("dials")).toObject());
        m_entries.insert(m_entries.begin(), {QStringLiteral("theme.json"), QJsonDocument(manifest).toJson(QJsonDocument::Indented)});
        if (!m_options.credits.isEmpty())
            m_entries.push_back({QStringLiteral("credits.txt"), m_options.credits.toUtf8()});
        addWallpapers(theme);
        return finish();
    }

private:
    bool load()
    {
        QByteArray bytes;
        QFile in(m_options.pagesFile);
        if (in.open(QIODevice::ReadOnly))
            bytes = in.read(MaxPagesFileBytes + 1);
        else
            bytes = preparePagesJson(initialPagesJson());
        const PagesParseResult parsed = parsePagesConfig(bytes);
        if (!parsed.config) {
            m_errors = parsed.errors;
            m_errors.prepend(QStringLiteral("the pages file has errors:"));
            return false;
        }
        m_config = *parsed.config;
        m_root = QJsonDocument::fromJson(bytes).object();
        if (m_options.themeIndex < 0 || static_cast<std::size_t>(m_options.themeIndex) >= m_config.themes.size()) {
            m_errors.push_back(QStringLiteral("there is no such theme"));
            return false;
        }
        return true;
    }

    // Reads the pictures the keys use, re-encodes them, and rewrites the keys' picture names in place.
    void collectPictures(QJsonArray &keys, const QString &page)
    {
        const QString folder = QFileInfo(m_options.pagesFile).absolutePath();
        QHash<QString, QString> names;
        QStringList taken;
        for (const auto &key : std::as_const(keys)) {
            for (const QString &name : fileIconNames(key.toObject().value(QStringLiteral("icon")))) {
                if (names.contains(name))
                    continue;
                QString portable = portableName(name, page);
                if (taken.contains(portable))
                    portable = name; // two pictures would meet: keep the full name for this one
                const QString base = portable.left(portable.lastIndexOf(QLatin1Char('.')));
                portable = base + QStringLiteral(".png");
                taken.push_back(portable);
                names.insert(name, portable);
                addIcon(QDir(folder).filePath(name), name, portable);
            }
        }
        for (auto entry : keys) {
            QJsonObject key = entry.toObject();
            if (key.contains(QStringLiteral("icon")))
                key.insert(QStringLiteral("icon"), renamedIcon(key.value(QStringLiteral("icon")), names));
            entry = key;
        }
    }

    void addIcon(const QString &path, const QString &shown, const QString &portable)
    {
        const QFileInfo info(path);
        if (!info.isFile() || info.isSymLink() || info.size() > MaxSourceBytes) {
            m_errors.push_back(QStringLiteral("the picture %1 is missing or cannot be used").arg(shown));
            return;
        }
        QFile in(path);
        if (!in.open(QIODevice::ReadOnly)) {
            m_errors.push_back(QStringLiteral("cannot read the picture %1").arg(shown));
            return;
        }
        const auto reencoded = reencodeIcon(in.read(MaxSourceBytes + 1));
        if (!reencoded)
            m_errors.push_back(QStringLiteral("the picture %1 is not an image that can be read").arg(shown));
        else
            m_entries.push_back({QStringLiteral("icons/") + portable, *reencoded});
    }

    // The tab's own wallpaper first (when it can be found), then the files the author added.
    QStringList wallpaperPaths(const Theme &theme)
    {
        QStringList paths;
        if (!theme.wallpaper.isEmpty() && !m_options.wallpaperRoots.isEmpty()) {
            const WallpaperCatalog catalog(m_options.wallpaperRoots);
            if (const WallpaperEntry *entry = catalog.find(theme.wallpaper))
                paths.push_back(entry->path);
            else
                m_warnings.push_back(QStringLiteral("the wallpaper of this tab (%1) was not found and is not in the theme").arg(theme.wallpaper));
        }
        for (const QString &path : m_options.wallpaperFiles) {
            if (!paths.contains(path))
                paths.push_back(path);
        }
        return paths;
    }

    void addWallpapers(const Theme &theme)
    {
        const QStringList paths = wallpaperPaths(theme);
        if (paths.size() > MaxThemeWallpapers) {
            m_errors.push_back(QStringLiteral("at most %1 wallpapers").arg(MaxThemeWallpapers));
            return;
        }
        const int quality = std::max(m_options.wallpaperQuality, MinWallpaperQuality);
        int n = 1;
        for (const QString &path : paths) {
            const QFileInfo info(path);
            QFile in(path);
            if (!info.isFile() || info.size() > MaxSourceBytes || !in.open(QIODevice::ReadOnly)) {
                m_errors.push_back(QStringLiteral("cannot read the wallpaper %1").arg(info.fileName()));
                continue;
            }
            const auto reencoded = reencodeWallpaper(in.read(MaxSourceBytes + 1), quality);
            if (!reencoded) {
                m_errors.push_back(QStringLiteral("the wallpaper %1 is not an image that can be read").arg(info.fileName()));
                continue;
            }
            m_entries.push_back({QStringLiteral("wallpapers/wallpaper-%1.jpg").arg(n, 2, 10, QLatin1Char('0')), *reencoded});
            ++n;
        }
    }

    ThemeExportResult finish()
    {
        ThemeExportResult result;
        if (m_errors.isEmpty()) {
            // Never produce what the importer would refuse.
            const ThemeParseResult check = parseThemeEntries(m_entries);
            if (!check.package) {
                m_errors = check.errors;
            } else {
                QString problem;
                if (const auto zip = writeStoredZip(m_entries, &problem))
                    result.file = *zip;
                else
                    m_errors.push_back(problem);
            }
        }
        result.errors = m_errors;
        result.warnings = m_warnings;
        return result;
    }

    const ThemeExportOptions &m_options;
    PagesConfig m_config;
    QJsonObject m_root;
    std::vector<ZipEntry> m_entries;
    QStringList m_errors;
    QStringList m_warnings;
};

} // namespace

std::optional<QByteArray> reencodeIcon(const QByteArray &picture)
{
    auto image = decode(picture);
    if (!image)
        return std::nullopt;
    if (image->width() > ThemeIconSize || image->height() > ThemeIconSize)
        image = image->scaled(ThemeIconSize, ThemeIconSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    return encode(image->convertToFormat(QImage::Format_ARGB32), "png", -1);
}

std::optional<QByteArray> reencodeWallpaper(const QByteArray &picture, int quality)
{
    auto image = decode(picture);
    if (!image)
        return std::nullopt;
    if (image->width() > ThemeWallpaperMaxWidth || image->height() > ThemeWallpaperMaxHeight)
        image = image->scaled(ThemeWallpaperMaxWidth, ThemeWallpaperMaxHeight, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    return encode(image->convertToFormat(QImage::Format_RGB32), "jpeg", std::clamp(quality, MinWallpaperQuality, 100));
}

ThemeExportResult exportTheme(const ThemeExportOptions &options)
{
    return Exporter(options).run();
}

} // namespace galleond
