// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QByteArray>
#include <QString>

#include <optional>

namespace galleond {

// The keyboard's brightness as galleond last saw it, published in a small file that Bosun watches. It is the
// way back from the daemon to the settings window on every system (Windows has no reply channel on its local
// endpoint; Linux also has the D-Bus signals). Written by galleond only, after a read or a confirmed write of
// the keyboard's brightness; read by Bosun, which never opens the device.
//
// The file is untrusted input for its reader (it lives in the user's config folder): a bounded size, exactly
// the three keys below with whole numbers in range, or nothing at all.
//   {"version":1,"brightness":700,"time":1783938123}
// `brightness` is what the keyboard reported or confirmed, 0 to 1000 (not necessarily a step of 100: another
// program may have set it); `time` is when galleond saw it, in seconds since 1970, so a reader can ignore a
// value older than its own start.
struct KeyboardState
{
    int brightness = 0;
    qint64 time = 0;
    friend bool operator==(const KeyboardState &, const KeyboardState &) = default;
};

inline constexpr qint64 MaxKeyboardStateBytes = 4096;

[[nodiscard]] QByteArray encodeKeyboardState(const KeyboardState &state);
[[nodiscard]] std::optional<KeyboardState> parseKeyboardState(const QByteArray &bytes);

// A state is worth showing only if galleond saw it at or after `notBefore` (seconds): an older file says what the
// keyboard held in an earlier session, which another program may have changed since.
[[nodiscard]] bool isFreshKeyboardState(const KeyboardState &state, qint64 notBefore);

// ~/.config/galleoncore/keyboard-state.json (the config folder of the platform), next to pages.json.
[[nodiscard]] QString keyboardStatePath();

// Atomic (written aside, then renamed), so a reader never sees half a file. Creates the folder. False on failure.
[[nodiscard]] bool writeKeyboardState(const QString &path, const KeyboardState &state);
// Nothing for a missing, unreadable, oversized or invalid file.
[[nodiscard]] std::optional<KeyboardState> readKeyboardState(const QString &path);

} // namespace galleond
