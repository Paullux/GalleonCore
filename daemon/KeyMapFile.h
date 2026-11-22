// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <galleon/keyboard/KeyMap.h>

#include <QByteArray>
#include <QString>

#include <optional>

namespace galleond {

// The file that holds the map between the LEDs of the live frame and the keys they light (keymap.json). It describes the
// hardware, never its use: no key press, no order, no time. Like every file read from disk it is untrusted input: a bounded
// size, exactly the keys below, whole numbers in range, no repeated LED, no key lit twice, or nothing at all.
//
//   {"version":1,"device":"1b1c:2b0c","source":"how it was measured","firmware":"2.3.88",
//    "leds":[{"led":58,"kind":"key","key":58},{"led":130,"kind":"strip","position":1},{"led":0,"kind":"unlit"}]}
//
// - `device` is the USB id of the keyboard part; any other value is refused.
// - `source` says how the answers were obtained (1 to MaxKeyMapSource plain characters). It is required: a map without a
//   source is not a measurement (AGENTS.md: never fill a value without a measurement or a cited source).
// - `firmware` is optional, as the person read it.
// - `leds` lists ONLY the LEDs someone has seen. A LED that is not listed has not been measured: there is no placeholder
//   value, absence is the "null until measured" of data/devices/.
// - `key` is the USB HID usage of the key (the identity of KeyIdentity.h); `position` counts the light strip's sections
//   from 1 at the left.
struct KeyMapFile
{
    galleon::keyboard::KeyMap map;
    QString source;
    QString firmware;
};

inline constexpr qint64 MaxKeyMapBytes = 65536;
inline constexpr int MaxKeyMapSource = 1000;
inline constexpr int MaxKeyMapFirmware = 40;

[[nodiscard]] QByteArray encodeKeyMap(const KeyMapFile &file);
// nullopt for anything out of shape; `why`, when given, receives a short reason for the log (never the file's content).
[[nodiscard]] std::optional<KeyMapFile> parseKeyMap(const QByteArray &bytes, QString *why = nullptr);

// ~/.config/galleoncore/keymap.json (the config folder of the platform).
[[nodiscard]] QString keyMapPath();
// Atomic (written aside, then renamed). Creates the folder. False on failure.
[[nodiscard]] bool writeKeyMap(const QString &path, const KeyMapFile &file);
// The measured map compiled into the program (data/devices/galleon-100-sd-keymap.json): which LED lights which key. Read once; empty (no LED known)
// if the resource could not be read or is not valid, which a test catches. It names only what was measured: a key that is not in it has no LED here.
[[nodiscard]] const galleon::keyboard::KeyMap &builtInKeyMap();
// Nothing for a missing, unreadable, oversized or invalid file.
[[nodiscard]] std::optional<KeyMapFile> readKeyMap(const QString &path, QString *why = nullptr);

} // namespace galleond
