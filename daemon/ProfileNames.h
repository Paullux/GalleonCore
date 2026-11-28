// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

// The names of the profiles of the module (see Profiles.h), alone: no file, no folder. The local endpoint, which a name can reach from outside,
// uses only this, so that checking a name needs nothing else.

#include <QString>

namespace galleond {

inline constexpr int MaxProfileNameLength = 32;
inline constexpr int MaxProfiles = 16; // with "default"

// The name of the profile that is the base pages file.
[[nodiscard]] QString defaultProfileName();

// "default", or 1 to 32 characters among a to z, 0 to 9 and "-", the first one not a "-". No path separator, no dot, no capital letter,
// no space: a name that is valid can only be one file of the `profiles` folder.
[[nodiscard]] bool isValidProfileName(const QString &name);

} // namespace galleond
