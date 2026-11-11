// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

// Installs a `.gctheme` file for the user: reads it, validates it completely
// (ThemePackage), works out the changes (ThemeInstallPlan), writes the pictures
// and then edits the pages file in one validated write (PagesEditor). The
// order matters: if the pages file cannot be changed, the pictures written for
// it are taken away again, so a failed install leaves nothing behind. A running
// galleond reads the new pages file by itself.
//
// Files: icons go in the folder of the pages file (where galleond looks for
// them); wallpapers go in `<wallpaperRoot>/<style>/`, the user's wallpaper
// folder. Both are written with the user's own permissions, never executable.

#include <QString>
#include <QStringList>

namespace galleond {

struct ThemeInstallResult
{
    bool ok = false;
    QStringList errors;
    QStringList warnings; // the install went through, but...
    QString page;
    QString themeName;
    int themeIndex = -1; // the new tab's place, to show it
};

struct ThemeInstallOptions
{
    QString themeFile;
    QString pagesFile;
    QString wallpaperRoot;
    bool applyDials = false;
};

[[nodiscard]] ThemeInstallResult installTheme(const ThemeInstallOptions &options);

} // namespace galleond
