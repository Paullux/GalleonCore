// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

// What the settings window does to the profile files (see Profiles.h): copy one into a new profile, rename one, delete one. Nothing here
// decides which profile is shown (that is galleond's, and the person's) and nothing opens the module.
//
// Every name is checked by isValidProfileName before a path is built from it, and the base profile `default` (pages.json) can be copied but
// never renamed or deleted. A deleted profile is **moved** to `profiles/.deleted`, never erased, so that a mistake loses nothing.

#include <QByteArray>
#include <QString>

namespace galleond {

// Why a change of the profile files was refused, for the window to say it in the language of the person (the English text of `error` is for the logs).
enum class ProfileError : unsigned char {
    None,
    BadName,       // the new name is not "a to z, 0 to 9 and -", or is "default"
    BaseProfile,   // "default" cannot be renamed or deleted
    NameTaken,     // a profile of this name already exists
    TooMany,       // there are already MaxProfiles
    Missing,       // the profile does not exist (or, to copy, cannot be read)
    TooLarge,      // the profile to copy is larger than a pages file may be
    WriteFailed,   // the new file could not be written, or the rename or the move failed
};

struct ProfileResult
{
    bool ok = false;
    ProfileError code = ProfileError::None;
    QString error; // why not, in English, for the logs (empty when ok)
};

// The name of the file a deleted profile is moved to: "<name>-<stamp>.json", with "-<attempt>" before ".json" for the second try on and later.
// Pure.
[[nodiscard]] QString deletedProfileFileName(const QString &name, const QString &stamp, int attempt);

// Makes the profile `name` from a copy of the file of the profile `from`. Refused when `name` is not valid or is "default", when it already
// exists, when there are already MaxProfiles, when `from` is not valid or its file is missing (only "default" may be missing: its `fallback` is then
// the content, the built-in pages, and an empty `fallback` refuses) or larger than a pages file may be. The copy is written whole or not at all,
// and is the person's own (not writable by others).
[[nodiscard]] ProfileResult createProfile(const QString &configFolder, const QString &name, const QString &from, const QByteArray &fallback);

// Gives the profile `from` the name `to`. Refused for "default" (either side), a name that is not valid, a profile that does not exist, a name that is
// taken.
[[nodiscard]] ProfileResult renameProfile(const QString &configFolder, const QString &from, const QString &to);

// Moves the file of the profile `name` to `profiles/.deleted`, under a name that is not taken (`stamp` is the time, as text). Refused for "default", a
// name that is not valid, a profile that does not exist.
[[nodiscard]] ProfileResult deleteProfile(const QString &configFolder, const QString &name, const QString &stamp);

} // namespace galleond
