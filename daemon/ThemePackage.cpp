// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ThemePackage.h"

#include "DesktopEntries.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QRegularExpression>
#include <QSet>

#include <algorithm>
#include <array>

namespace galleond {

namespace {

const QRegularExpression &licensePattern()
{
    static const QRegularExpression pattern(QStringLiteral("^[A-Za-z0-9.+-]{1,40}$"));
    return pattern;
}

const QRegularExpression &idPattern()
{
    static const QRegularExpression pattern(QStringLiteral("^[a-z0-9_-]{1,32}$"));
    return pattern;
}

const QRegularExpression &iconFilePattern()
{
    static const QRegularExpression pattern(QStringLiteral("^[A-Za-z0-9._-]{1,64}\\.png$"));
    return pattern;
}

const QRegularExpression &wallpaperFilePattern()
{
    static const QRegularExpression pattern(QStringLiteral("^[A-Za-z0-9._-]{1,64}\\.(jpg|jpeg)$"));
    return pattern;
}

const QRegularExpression &themeIconNamePattern()
{
    static const QRegularExpression pattern(QStringLiteral("^[A-Za-z0-9_-][A-Za-z0-9._-]{0,63}$"));
    return pattern;
}

// True when the text has no control character (a name or a description is shown to people).
bool printable(const QString &text)
{
    return std::ranges::none_of(text, [](QChar c) { return c.unicode() < 0x20 || c.unicode() == 0x7F; });
}

std::optional<quint32> readBe32(QByteArrayView bytes, qsizetype at)
{
    if (at < 0 || at > bytes.size() - 4)
        return std::nullopt;
    return (static_cast<quint32>(static_cast<quint8>(bytes[at])) << 24) | (static_cast<quint32>(static_cast<quint8>(bytes[at + 1])) << 16)
        | (static_cast<quint32>(static_cast<quint8>(bytes[at + 2])) << 8) | static_cast<quint32>(static_cast<quint8>(bytes[at + 3]));
}

std::optional<ImageInfo> inspectPng(QByteArrayView bytes)
{
    static constexpr std::array<unsigned char, 8> signature{0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};
    if (bytes.size() < 33 || !std::equal(signature.begin(), signature.end(), bytes.begin(), [](unsigned char a, char b) {
            return a == static_cast<unsigned char>(b);
        }))
        return std::nullopt;
    // The first chunk must be IHDR, 13 bytes long: width, height, ...
    if (readBe32(bytes, 8) != 13U || bytes.sliced(12, 4) != QByteArrayView("IHDR"))
        return std::nullopt;
    const auto width = readBe32(bytes, 16);
    const auto height = readBe32(bytes, 20);
    if (!width || !height || *width == 0 || *height == 0 || *width > 0x7FFFFFFFU || *height > 0x7FFFFFFFU)
        return std::nullopt;
    return ImageInfo{ImageInfo::Kind::Png, static_cast<int>(*width), static_cast<int>(*height)};
}

std::optional<ImageInfo> inspectJpeg(QByteArrayView bytes)
{
    const auto byteAt = [&bytes](qsizetype at) { return static_cast<quint8>(bytes[at]); };
    if (bytes.size() < 4 || byteAt(0) != 0xFF || byteAt(1) != 0xD8)
        return std::nullopt;
    qsizetype at = 2;
    while (at + 4 <= bytes.size()) {
        if (byteAt(at) != 0xFF)
            return std::nullopt;
        const quint8 marker = byteAt(at + 1);
        if (marker == 0xFF) { // fill byte
            ++at;
            continue;
        }
        if (marker == 0xD8 || (marker >= 0xD0 && marker <= 0xD7) || marker == 0x01) { // markers without a length
            at += 2;
            continue;
        }
        if (marker == 0xD9 || marker == 0xDA) // end of image, start of scan: no frame header before it
            return std::nullopt;
        const qsizetype length = (byteAt(at + 2) << 8) | byteAt(at + 3);
        if (length < 2 || at + 2 + length > bytes.size())
            return std::nullopt;
        const bool frame = marker >= 0xC0 && marker <= 0xCF && marker != 0xC4 && marker != 0xC8 && marker != 0xCC;
        if (frame) {
            if (length < 8)
                return std::nullopt;
            const int height = (byteAt(at + 5) << 8) | byteAt(at + 6);
            const int width = (byteAt(at + 7) << 8) | byteAt(at + 8);
            if (width == 0 || height == 0)
                return std::nullopt;
            return ImageInfo{ImageInfo::Kind::Jpeg, width, height};
        }
        at += 2 + length;
    }
    return std::nullopt;
}

// Collects the problems of one package; the checks are small methods so that each has one job.
class Reader
{
public:
    explicit Reader(const std::vector<ZipEntry> &entries) : m_entries(entries) {}

    ThemeParseResult run()
    {
        sortEntries();
        if (!m_manifest.has_value()) {
            error(QStringLiteral("theme.json"), QStringLiteral("the theme has no theme.json"));
            return finish();
        }
        if (m_manifest->size() > MaxThemeManifestBytes) {
            error(QStringLiteral("theme.json"), QStringLiteral("is larger than %1 KiB").arg(MaxThemeManifestBytes / 1024));
            return finish();
        }
        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(*m_manifest, &parseError);
        if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
            error(QStringLiteral("theme.json"), QStringLiteral("is not a JSON object"));
            return finish();
        }
        manifest(document.object());
        checkImages();
        checkFilesAreUsed();
        return finish();
    }

private:
    void error(const QString &path, const QString &problem) { m_errors.push_back(path + QStringLiteral(": ") + problem); }

    ThemeParseResult finish()
    {
        ThemeParseResult result;
        if (m_errors.isEmpty() && m_config.has_value()) {
            ThemePackage package;
            package.meta = m_meta;
            package.config = *m_config;
            package.appAlternatives = m_alternatives;
            package.keysJson = m_keysJson;
            package.dialsJson = m_dialsJson;
            package.icons = m_icons;
            package.wallpapers = m_wallpapers;
            result.package = std::move(package);
        } else {
            result.errors = m_errors;
            if (result.errors.isEmpty())
                result.errors.push_back(QStringLiteral("the theme could not be read"));
        }
        return result;
    }

    void sortEntries()
    {
        for (const ZipEntry &entry : m_entries) {
            if (entry.name == QLatin1String("theme.json")) {
                m_manifest = entry.data;
            } else if (entry.name == QLatin1String("credits.txt")) {
                credits(entry);
            } else if (entry.name.startsWith(QLatin1String("icons/"))) {
                file(entry, QStringLiteral("icons/"), iconFilePattern(), m_icons, MaxThemeIcons, QStringLiteral("icons"));
            } else if (entry.name.startsWith(QLatin1String("wallpapers/"))) {
                file(entry, QStringLiteral("wallpapers/"), wallpaperFilePattern(), m_wallpapers, MaxThemeWallpapers, QStringLiteral("wallpapers"));
            } else {
                error(entry.name, QStringLiteral("a file a theme does not have"));
            }
        }
    }

    void credits(const ZipEntry &entry)
    {
        if (entry.data.size() > MaxThemeCreditsBytes) {
            error(entry.name, QStringLiteral("is larger than %1 KiB").arg(MaxThemeCreditsBytes / 1024));
            return;
        }
        // Text for people: valid UTF-8 without control characters other than line breaks and tabs.
        const QString text = QString::fromUtf8(entry.data);
        const bool clean = text.toUtf8() == entry.data
            && std::ranges::none_of(text, [](QChar c) { return (c.unicode() < 0x20 && c != u'\n' && c != u'\r' && c != u'\t') || c.unicode() == 0x7F; });
        if (!clean) {
            error(entry.name, QStringLiteral("must be plain UTF-8 text"));
            return;
        }
        m_meta.credits = text;
    }

    void file(const ZipEntry &entry, const QString &folder, const QRegularExpression &pattern, std::vector<ZipEntry> &into, int maximum,
              const QString &what)
    {
        const QString name = entry.name.mid(folder.size());
        if (!pattern.match(name).hasMatch()) {
            error(entry.name, QStringLiteral("not an accepted file name for %1").arg(what));
            return;
        }
        if (static_cast<int>(into.size()) >= maximum) {
            error(entry.name, QStringLiteral("more than %1 files in %2").arg(maximum).arg(what));
            return;
        }
        into.push_back({name, entry.data});
    }

    // ---- the manifest ----

    void manifest(const QJsonObject &root)
    {
        static const QStringList known = {QStringLiteral("formatVersion"), QStringLiteral("id"),    QStringLiteral("name"),
                                          QStringLiteral("author"),        QStringLiteral("license"), QStringLiteral("description"),
                                          QStringLiteral("color"),         QStringLiteral("keys"),  QStringLiteral("dials")};
        for (auto it = root.begin(); it != root.end(); ++it) {
            if (!known.contains(it.key()))
                error(QStringLiteral("theme.json"), QStringLiteral("unknown field \"%1\"").arg(it.key().left(40)));
        }
        if (root.value(QStringLiteral("formatVersion")).toInt(-1) != ThemeFormatVersion || !root.value(QStringLiteral("formatVersion")).isDouble())
            error(QStringLiteral("formatVersion"), QStringLiteral("must be %1 (this version of GalleonCore does not know another)").arg(ThemeFormatVersion));
        QString name;
        identity(root, name);
        QString color;
        text(root, QStringLiteral("color"), 7, color, true);
        if (!root.value(QStringLiteral("keys")).isArray())
            error(QStringLiteral("keys"), QStringLiteral("must be an array"));
        else
            m_hasKeys = true;
        if (!m_errors.isEmpty() || !m_hasKeys)
            return;

        QJsonArray keys = root.value(QStringLiteral("keys")).toArray();
        stripAlternatives(keys);
        m_keysJson = keys;
        m_dialsJson = root.value(QStringLiteral("dials")).toObject();
        QJsonObject pages;
        pages.insert(m_meta.id, QJsonObject{{QStringLiteral("keys"), keys}});
        const QJsonObject theme{{QStringLiteral("name"), name}, {QStringLiteral("page"), m_meta.id}, {QStringLiteral("color"), color}};
        QJsonObject pagesFile{{QStringLiteral("version"), 1},
                              {QStringLiteral("home"), m_meta.id},
                              {QStringLiteral("pages"), pages},
                              {QStringLiteral("themes"), QJsonArray{theme}}};
        if (root.contains(QStringLiteral("dials")))
            pagesFile.insert(QStringLiteral("dials"), root.value(QStringLiteral("dials")));

        const PagesParseResult parsed = parsePagesConfig(QJsonDocument(pagesFile).toJson(QJsonDocument::Compact));
        if (!parsed.config) {
            for (const QString &problem : parsed.errors)
                error(QStringLiteral("theme.json"), problem);
            return;
        }
        m_config = *parsed.config;
        checkActions(*parsed.config);
        checkIconReferences(*parsed.config);
    }

    void identity(const QJsonObject &root, QString &name)
    {
        text(root, QStringLiteral("id"), 32, m_meta.id, true);
        if (!m_meta.id.isEmpty() && !idPattern().match(m_meta.id).hasMatch())
            error(QStringLiteral("id"), QStringLiteral("must be 1 to 32 characters among a-z, 0-9, _ and -"));
        text(root, QStringLiteral("name"), MaxLabelLength, name, true);
        text(root, QStringLiteral("author"), MaxThemeAuthorLength, m_meta.author, true);
        text(root, QStringLiteral("license"), MaxThemeLicenseLength, m_meta.license, true);
        if (!m_meta.license.isEmpty() && !licensePattern().match(m_meta.license).hasMatch())
            error(QStringLiteral("license"), QStringLiteral("must be a licence identifier such as CC-BY-SA-4.0"));
        text(root, QStringLiteral("description"), MaxThemeDescriptionLength, m_meta.description, false);
    }

    void text(const QJsonObject &root, const QString &field, int maximum, QString &out, bool required)
    {
        const QJsonValue value = root.value(field);
        if (value.isUndefined()) {
            if (required)
                error(field, QStringLiteral("is required"));
            return;
        }
        if (!value.isString()) {
            error(field, QStringLiteral("must be text"));
            return;
        }
        const QString string = value.toString().trimmed();
        if (string.isEmpty() && required) {
            error(field, QStringLiteral("is required"));
        } else if (string.size() > maximum) {
            error(field, QStringLiteral("is longer than %1 characters").arg(maximum));
        } else if (!printable(string)) {
            error(field, QStringLiteral("has a control character"));
        } else {
            out = string;
        }
    }

    // `alternatives` is the package's own addition to an app action: take it out
    // before the pages file parser, which does not know it, and keep it apart.
    void stripAlternatives(QJsonArray &keys)
    {
        for (int i = 0; i < keys.size(); ++i) {
            if (!keys.at(i).isObject())
                continue;
            QJsonObject key = keys.at(i).toObject();
            QJsonObject action = key.value(QStringLiteral("action")).toObject();
            if (!action.contains(QStringLiteral("alternatives")))
                continue;
            const QJsonValue list = action.value(QStringLiteral("alternatives"));
            action.remove(QStringLiteral("alternatives"));
            key.insert(QStringLiteral("action"), action);
            keys[i] = key;
            const QString path = QStringLiteral("keys[%1].action.alternatives").arg(i);
            if (action.value(QStringLiteral("type")).toString() != QLatin1String("app")) {
                error(path, QStringLiteral("only an app action has alternatives"));
                continue;
            }
            if (!list.isArray() || list.toArray().isEmpty() || list.toArray().size() > MaxAppAlternatives) {
                error(path, QStringLiteral("must be a list of 1 to %1 desktop file names").arg(MaxAppAlternatives));
                continue;
            }
            QStringList names;
            for (const auto item : list.toArray()) {
                const QString name = item.toString();
                if (!item.isString() || !isPackageDesktopId(name))
                    error(path, QStringLiteral("each must be a desktop file name such as firefox.desktop"));
                else
                    names.push_back(name);
            }
            if (!names.isEmpty())
                m_alternatives.insert(i, names);
        }
    }

    static bool isPackageDesktopId(const QString &name)
    {
        return name.endsWith(QLatin1String(".desktop")) && !name.contains(QLatin1Char('/')) && !name.contains(QLatin1Char('\\'))
            && DesktopEntries::isValidReference(name);
    }

    // ---- what a package may do ----

    void checkAction(const Action &action, const QString &path)
    {
        switch (action.type) {
        case ActionType::None:
        case ActionType::Media:
        case ActionType::Back:
        case ActionType::Key:
        case ActionType::Steam:
        case ActionType::WallpaperMode:
            return;
        case ActionType::Page:
            if (action.page != m_meta.id)
                error(path, QStringLiteral("a theme can only open its own page"));
            return;
        case ActionType::Theme:
            error(path, QStringLiteral("a theme cannot switch themes"));
            return;
        case ActionType::App:
            if (!isPackageDesktopId(action.desktop))
                error(path, QStringLiteral("an application is named by its desktop file name, never by a path"));
            return;
        }
    }

    void checkActions(const PagesConfig &config)
    {
        for (const auto &key : config.pages.value(m_meta.id).keys) {
            if (key)
                checkAction(key->action, QStringLiteral("keys[%1].action").arg(key->key));
        }
        int dial = 0;
        for (const DialConfig &config_dial : config.dials) {
            checkAction(config_dial.turn, QStringLiteral("dials.%1.turn").arg(dial));
            checkAction(config_dial.press, QStringLiteral("dials.%1.press").arg(dial));
            ++dial;
        }
    }

    // ---- pictures ----

    // An icon with an extension is a file of the package; one without is a name of the desktop's icon theme.
    void checkIconReferences(const PagesConfig &config)
    {
        QSet<QString> available;
        for (const ZipEntry &icon : m_icons)
            available.insert(icon.name);
        for (const auto &key : config.pages.value(m_meta.id).keys) {
            if (!key)
                continue;
            QStringList names{key->icon.name};
            names.append(key->icon.byState.values());
            for (const QString &name : names) {
                if (name.isEmpty())
                    continue;
                if (name.contains(QLatin1Char('.'))) {
                    if (!available.contains(name))
                        error(QStringLiteral("keys[%1].icon").arg(key->key), QStringLiteral("\"%1\" is not in icons/").arg(name));
                    else
                        m_usedIcons.insert(name);
                } else if (!themeIconNamePattern().match(name).hasMatch()) {
                    error(QStringLiteral("keys[%1].icon").arg(key->key), QStringLiteral("\"%1\" is not an icon name").arg(name));
                }
            }
        }
    }

    void checkImages()
    {
        for (const ZipEntry &icon : m_icons) {
            const QString path = QStringLiteral("icons/") + icon.name;
            if (icon.data.size() > MaxThemeIconBytes) {
                error(path, QStringLiteral("is larger than %1 KiB").arg(MaxThemeIconBytes / 1024));
                continue;
            }
            const auto info = inspectPng(icon.data);
            if (!info)
                error(path, QStringLiteral("is not a PNG picture"));
            else if (info->width > MaxThemeIconDimension || info->height > MaxThemeIconDimension)
                error(path, QStringLiteral("is larger than %1 x %1 pixels").arg(MaxThemeIconDimension));
        }
        for (const ZipEntry &wallpaper : m_wallpapers) {
            const QString path = QStringLiteral("wallpapers/") + wallpaper.name;
            if (wallpaper.data.size() > MaxThemeWallpaperBytes) {
                error(path, QStringLiteral("is larger than %1 MiB").arg(MaxThemeWallpaperBytes / ZipMiB));
                continue;
            }
            const auto info = inspectJpeg(wallpaper.data);
            if (!info)
                error(path, QStringLiteral("is not a JPEG picture"));
            else if (info->width > MaxThemeWallpaperDimension || info->height > MaxThemeWallpaperDimension)
                error(path, QStringLiteral("is larger than %1 x %1 pixels").arg(MaxThemeWallpaperDimension));
        }
    }

    void checkFilesAreUsed()
    {
        if (!m_config.has_value())
            return;
        for (const ZipEntry &icon : m_icons) {
            if (!m_usedIcons.contains(icon.name))
                error(QStringLiteral("icons/") + icon.name, QStringLiteral("no key uses this picture"));
        }
    }

    const std::vector<ZipEntry> &m_entries;
    std::optional<QByteArray> m_manifest;
    ThemeMeta m_meta;
    std::optional<PagesConfig> m_config;
    QHash<int, QStringList> m_alternatives;
    QJsonArray m_keysJson;
    QJsonObject m_dialsJson;
    std::vector<ZipEntry> m_icons;
    std::vector<ZipEntry> m_wallpapers;
    QSet<QString> m_usedIcons;
    QStringList m_errors;
    bool m_hasKeys = false;
};

} // namespace

std::optional<ImageInfo> inspectImage(QByteArrayView bytes)
{
    if (auto png = inspectPng(bytes))
        return png;
    return inspectJpeg(bytes);
}

ThemeParseResult parseThemeEntries(const std::vector<ZipEntry> &entries)
{
    return Reader(entries).run();
}

ThemeParseResult parseThemePackage(const QByteArray &file)
{
    const ZipReadResult archive = readStoredZip(file);
    if (!archive.errors.isEmpty()) {
        ThemeParseResult result;
        result.errors = archive.errors;
        return result;
    }
    return parseThemeEntries(archive.entries);
}

} // namespace galleond
