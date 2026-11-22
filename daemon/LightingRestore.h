// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QByteArray>
#include <QString>

#include <optional>

namespace galleond {

// Putting the live lighting back when galleond starts, if the person asked for it: the decisions and the two small files.
//
// Two files, each with one writer, so that the settings window never writes what galleond remembers:
//
//   lighting-restore.json  {"version":1,"restore":true}      written by Bosun: the choice of the person, OFF unless they made it
//   lighting-memory.json   {"version":1,"memory":"clean"}    written by galleond ONLY: how the lighting stood when galleond last ended
//
// `memory` is "off" (the lighting was off, or ended in any way but a clean exit: refused, failed, watchdog died, the keys could not be confirmed back),
// "running" (it was on, and nothing has said that galleond ended properly: a crash leaves this) or "clean" (it was on and galleond ended properly, the keys
// given back and confirmed). The lighting is put back only when the person asked AND the memory says "clean": after a crash, a freeze or a failure it
// stays off until asked again by hand. The memory is put back to "off" as soon as it has been read, so that a start that crashes is not repeated.
// Nothing here is about key presses, and nothing is written to the keyboard: the files say only "on" or "off".
// Like every file read from disk both are untrusted: a bounded size, exactly the keys above, a known value, or nothing at all.
enum class LightingMemory : unsigned char { Off, Running, CleanExit };

inline constexpr qint64 MaxLightingRestoreBytes = 256;
// How long after galleond starts the lighting is put back, so that the session, the sleep events and the keyboard are there.
inline constexpr int LightingRestoreDelayMs = 3000;

[[nodiscard]] QByteArray encodeLightingRestore(bool restore);
[[nodiscard]] std::optional<bool> parseLightingRestore(const QByteArray &bytes, QString *why = nullptr);
[[nodiscard]] QByteArray encodeLightingMemory(LightingMemory memory);
[[nodiscard]] std::optional<LightingMemory> parseLightingMemory(const QByteArray &bytes, QString *why = nullptr);

// ~/.config/galleoncore/lighting-restore.json and lighting-memory.json (the config folder of the platform).
[[nodiscard]] QString lightingRestorePath();
[[nodiscard]] QString lightingMemoryPath();
// Atomic (written aside, then renamed). False on failure.
[[nodiscard]] bool writeLightingRestore(const QString &path, bool restore);
[[nodiscard]] bool writeLightingMemory(const QString &path, LightingMemory memory);
// Nothing for a missing, unreadable, oversized or invalid file.
[[nodiscard]] std::optional<bool> readLightingRestore(const QString &path, QString *why = nullptr);
[[nodiscard]] std::optional<LightingMemory> readLightingMemory(const QString &path, QString *why = nullptr);

// The decision, pure: the person asked, and galleond ended properly with the lighting on.
[[nodiscard]] bool shouldRestoreLighting(std::optional<bool> asked, std::optional<LightingMemory> memory);

// At the start of galleond: reads both files, puts the memory back to "off" at once, and says whether the lighting is to be put back. If the memory
// cannot be written, nothing is put back (a crash could not then be told from a clean exit). Only decides: switching on, with all its refusals, is the
// caller's (LightingController::Enable).
[[nodiscard]] bool consumeLightingRestore(const QString &restorePath, const QString &memoryPath);

} // namespace galleond
