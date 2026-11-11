// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

// A shareable theme: the keys of one page, the colour that goes with it, what
// the two dials do, and the pictures it needs, in a `.gc.theme` file. The file
// is a zip archive of stored entries (see StoredZip.h):
//
//   theme.json           the manifest (required)
//   credits.txt          free text for the author's notes and credits (optional, <= 16 KiB)
//   icons/<name>.png     the key pictures the manifest names by file name
//   wallpapers/<n>.jpg   screen pictures offered by the theme (JPEG only)
//
// The manifest speaks the same language as the pages file, for one page:
//
//   { "formatVersion": 1, "id": "discord", "name": "Discord", "author": "A. Person",
//     "license": "CC-BY-SA-4.0", "description": "Voice and chat keys for Discord",
//     "color": "#5865F2",
//     "keys": [ { "key": 0, "label": "Mute", "icon": "mute.png",
//                 "action": { "type": "key", "key": "..." } },
//               { "key": 1, "label": "Discord", "icon": "discord",
//                 "action": { "type": "app", "desktop": "discord.desktop",
//                             "alternatives": ["com.discordapp.Discord.desktop", "discord_discord.desktop"] } } ],
//     "dials": { "0": { "turn": {...}, "press": {...} } } }
//
// A package is external data from someone else, not trusted until validated in
// full. Beyond what the pages file parser checks (it is run on the manifest,
// so a theme can never say what a page cannot), a package may only hold
// actions that cannot do harm by themselves: media commands, its own page,
// back, a keypad key from the fixed list, a Steam game, and an application
// named by desktop file ID, never by path. Every file must be used, be the
// kind it claims (checked by its content), and stay within its size.

#include "PagesConfig.h"
#include "StoredZip.h"

#include <QByteArray>
#include <QHash>
#include <QString>
#include <QStringList>

#include <optional>
#include <vector>

namespace galleond {

inline constexpr int ThemeFormatVersion = 1;
inline constexpr qsizetype MaxThemeManifestBytes = qsizetype{128} * 1024;
inline constexpr qsizetype MaxThemeCreditsBytes = qsizetype{16} * 1024;
inline constexpr qsizetype MaxThemeIconBytes = qsizetype{256} * 1024;
inline constexpr qsizetype MaxThemeWallpaperBytes = 2 * ZipMiB;
inline constexpr int MaxThemeIcons = 24;
inline constexpr int MaxThemeWallpapers = 4;
inline constexpr int MaxThemeIconDimension = 512;
inline constexpr int MaxThemeWallpaperDimension = 4096;
inline constexpr int MaxThemeAuthorLength = 64;
inline constexpr int MaxThemeLicenseLength = 40;
inline constexpr int MaxThemeDescriptionLength = 300;
inline constexpr int MaxAppAlternatives = 6;

struct ThemeMeta
{
    QString id; // becomes the page name when installed
    QString author;
    QString license; // an SPDX identifier such as "CC-BY-SA-4.0"; required
    QString description;
    QString credits; // credits.txt, when present
};

struct ThemePackage
{
    ThemeMeta meta;
    PagesConfig config; // exactly one page (named meta.id), one theme and the dials
    // Other desktop file IDs that start the same application on another kind of
    // installation (deb, Flatpak, Snap): by key number, in order of preference.
    QHash<int, QStringList> appAlternatives;
    std::vector<ZipEntry> icons;      // names without the folder: "mute.png"
    std::vector<ZipEntry> wallpapers; // names without the folder: "background.jpg"
};

struct ThemeParseResult
{
    std::optional<ThemePackage> package; // empty when `errors` is not
    QStringList errors;                  // one line each, with the path of the field
};

// Reads and validates a `.gc.theme` file. Never throws; any problem refuses it whole.
[[nodiscard]] ThemeParseResult parseThemePackage(const QByteArray &file);

// The same, from entries already read from the archive.
[[nodiscard]] ThemeParseResult parseThemeEntries(const std::vector<ZipEntry> &entries);

// What a picture says about itself, from its first bytes only (nothing is decoded).
struct ImageInfo
{
    enum class Kind : std::uint8_t { Png, Jpeg } kind = Kind::Png;
    int width = 0;
    int height = 0;
};
[[nodiscard]] std::optional<ImageInfo> inspectImage(QByteArrayView bytes);

} // namespace galleond
