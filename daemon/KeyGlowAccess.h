// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "AllowFile.h"

#include <QString>

#include <optional>

namespace galleond {

// The choice "the keys glow under the fingers" (key-glow.json): the reactive glow reads the key presses of the keyboard, so it is OFF unless the person made the
// choice, in Bosun, where its state is always shown (AGENTS.md, security and privacy by design, rule 5). One writer: Bosun writes it, galleond only reads it, and
// no program on the bus or the local endpoint can change it. The shape of the file, and how it is read as untrusted, are those of AllowFile.h: a missing,
// oversized or damaged file means off.
// ~/.config/galleoncore/key-glow.json (the config folder of the platform).
[[nodiscard]] QString keyGlowAccessPath();
[[nodiscard]] inline bool writeKeyGlowAccess(const QString &path, bool allow) { return writeAllowFile(path, allow); }
[[nodiscard]] inline std::optional<bool> readKeyGlowAccess(const QString &path, QString *why = nullptr) { return readAllowFile(path, why); }
// What the daemon acts on: true only for a file that says so.
[[nodiscard]] inline bool keyGlowAllowed(const QString &path) { return allowFileSaysYes(path); }

} // namespace galleond
