// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

// Makes a `.gctheme` file (see ThemePackage.h) from one theme of the user's
// pages file. The file is never trusted to be well made just because we made
// it: before anything is written, the result goes through the very validator
// the importer uses, so GalleonCore never produces a file it would refuse.
//
// What it does for the author:
// - keeps the theme's page only, with its colour; sub-pages are not part of a theme;
// - refuses actions a shared theme cannot hold (an application given by a path,
//   a link to another page, a theme switch), naming the keys, instead of
//   dropping them silently;
// - takes the key pictures from the pages file's folder, re-encodes them as PNG
//   (no larger than a key needs) and gives them a name without the page prefix;
// - re-encodes every wallpaper as JPEG, at a quality of at least 80 %, no larger
//   than the screen, which keeps files small and removes embedded metadata;
// - puts the tab's own wallpaper first, then those the author added (the first one becomes the
//   wallpaper of the tab when the theme is installed);
// - leaves the dials out unless asked (they are shared by every theme).

#include <galleon/Protocol.h>

#include <QByteArray>
#include <QString>
#include <QStringList>

#include <optional>

namespace galleond {

inline constexpr int MinWallpaperQuality = 80;
inline constexpr int DefaultWallpaperQuality = 85;
inline constexpr int ThemeWallpaperMaxWidth = galleon::protocol::ScreenWidth;   // 720: the screen's coordinate space
inline constexpr int ThemeWallpaperMaxHeight = galleon::protocol::ScreenHeight; // 384
inline constexpr int ThemeIconSize = galleon::protocol::KeyImageSize;           // 160: a key picture is never kept larger than this

struct ThemeExportOptions
{
    QString pagesFile;
    int themeIndex = 0;
    QString author;
    QString license; // SPDX identifier
    QString description;
    QString credits;
    QStringList wallpaperFiles; // files chosen by the author (JPEG or PNG), at most MaxThemeWallpapers
    QStringList wallpaperRoots; // the wallpaper folders, to find the tab's own wallpaper: it goes first
    int wallpaperQuality = DefaultWallpaperQuality; // never below MinWallpaperQuality
    bool includeDials = false;
};

struct ThemeExportResult
{
    std::optional<QByteArray> file; // the .gctheme content; empty when `errors` is not
    QStringList errors;
    QStringList warnings;
};

[[nodiscard]] ThemeExportResult exportTheme(const ThemeExportOptions &options);

// The pictures, on their own so that they are tested alone. Both refuse what they cannot read
// or what is too large to decode safely, and return nothing for it.
[[nodiscard]] std::optional<QByteArray> reencodeIcon(const QByteArray &picture);
[[nodiscard]] std::optional<QByteArray> reencodeWallpaper(const QByteArray &picture, int quality);

} // namespace galleond
