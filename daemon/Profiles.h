// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

// Profiles of the module: each one is a complete pages file (screen, screensaver, themes, pages, dials), and one of them is active. The
// profile "default" is the pages file that was always there (pages.json), so nothing is moved or changed for a person who never makes one;
// the others are in the folder `profiles` next to it, one file each, named after the profile.
//
// The name of a profile is the only thing that decides which file is read, and it can come from outside (a key of a pages file, a D-Bus
// call, the local endpoint of Windows): it is checked here, to a short and plain shape, before any path is built from it. Nothing here
// reads the active window or any key press: the person chooses.
//
// Pure functions, but for the few that look at the folder or write the small file that remembers the active profile.

#include "ProfileNames.h"

#include <QByteArray>
#include <QString>
#include <QStringList>

#include <optional>

namespace galleond {

inline constexpr qsizetype MaxActiveProfileFileBytes = 1024;

// The file of a profile in `configFolder` (the folder of pages.json): pages.json for "default", profiles/<name>.json for another. Empty
// for a name that is not valid.
[[nodiscard]] QString profilePath(const QString &configFolder, const QString &name);

// The profiles that a list of file names (without ".json") makes: "default" first, then the valid names, sorted, without duplicates, at
// most MaxProfiles in all. Pure.
[[nodiscard]] QStringList profileNamesFrom(const QStringList &fileBaseNames);

// The profiles of `configFolder`: those of the files in its `profiles` folder, as above.
[[nodiscard]] QStringList listProfiles(const QString &configFolder);

// The profile `steps` places after `active` in `names`, going round (negative: before). An `active` that is not in the list counts as
// the first one ("default"). `names` empty gives "default". Pure.
[[nodiscard]] QString steppedProfile(const QStringList &names, const QString &active, int steps);

// ---- the profile that is active, remembered in a small file (profile-active.json, next to pages.json) ----

[[nodiscard]] QString activeProfilePath(const QString &configFolder);
[[nodiscard]] QByteArray encodeActiveProfile(const QString &name);
// The profile of such a file, or nothing (and the reason) for a file that is too large, not an object, with other keys or another version,
// or a name that is not valid. The file is external data: it is never trusted.
[[nodiscard]] std::optional<QString> parseActiveProfile(const QByteArray &bytes, QString *why = nullptr);
[[nodiscard]] bool writeActiveProfile(const QString &configFolder, const QString &name);
// The profile to use at start: the one remembered if its name is valid and its file exists, else "default".
[[nodiscard]] QString activeProfileAtStart(const QString &configFolder);

} // namespace galleond
