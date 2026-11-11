// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

// What installing a validated theme (see ThemePackage.h) changes, worked out
// without touching a file: the page's name and the theme's name made unique,
// the pictures renamed so they cannot replace the user's own, the application
// of each key picked among those installed on this machine (deb, Flatpak or
// Snap), and the keys rewritten to match. Pure: it takes what it needs as
// arguments, so it is tested without a desktop. ThemeInstaller does the I/O.

#include "ThemePackage.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QStringList>

#include <functional>
#include <vector>

namespace galleond {

struct ThemeInstallContext
{
    QStringList existingPages;
    QStringList existingThemeNames;
    QStringList existingIconFiles;         // file names in the icon folder
    QStringList existingWallpaperStyles;   // folder names in the wallpaper folder
    int existingThemeCount = 0;
    // The literal reference to store for this candidate (a desktop file ID, a
    // path, or a resolved well-known app), or empty when it is not installed
    // here. A well-known candidate ("discord") never ends up stored as such:
    // this is what turns it into whatever this machine actually has.
    std::function<QString(const QString &)> resolveDesktop;
    bool applyDials = false; // the dials are shared by every theme: copy the theme's only when asked
};

struct ThemeInstallPlan
{
    QString page;      // the page it becomes
    QString themeName; // the tab it becomes
    QString color;     // "#rrggbb"
    QJsonObject pageObject; // { "keys": [...] } with renamed pictures and the chosen applications
    QJsonObject dials;      // empty unless ThemeInstallContext::applyDials
    std::vector<ZipEntry> iconFiles;      // the new names
    QString wallpaperStyle;               // the folder the wallpapers go in (empty: none)
    std::vector<ZipEntry> wallpaperFiles; // the new names
    QString wallpaper;                    // the tab's own wallpaper: the first one, by catalog name ("style/file.jpg")
    QStringList warnings;                 // not an error: "discord.desktop is not installed"
    QStringList errors;                   // the plan is unusable when not empty

    [[nodiscard]] bool ok() const { return errors.isEmpty(); }
};

[[nodiscard]] ThemeInstallPlan planThemeInstall(const ThemePackage &package, const ThemeInstallContext &context);

// `base` made unique among `taken` by a number at the end, keeping to `maxLength`.
[[nodiscard]] QString uniqueName(const QString &base, const QStringList &taken, int maxLength, const QString &separator);

} // namespace galleond
