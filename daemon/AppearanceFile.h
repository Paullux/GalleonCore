// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <galleon/keyboard/Appearance.h>

#include <QByteArray>
#include <QString>

#include <optional>

namespace galleond {

// The file that holds the appearance of the live lighting (lighting-appearance.json): what the keys look like, written by the settings window and
// applied by galleond when the live lighting is active. It holds a description of colours, never anything about the keys that were pressed. The sky
// may hold the place typed in its settings (a latitude and a longitude): it is personal data, never logged, and only read to find the sunrise. Like
// every file read from disk it is untrusted input: a bounded size, exactly the keys below, whole numbers in range, no LED twice, or nothing at all.
//
//   {"version":1,
//    "base":{"type":"uniform","colour":[0,0,0]},          or  {"type":"rainbow","speed":0}   or  {"type":"sky"}  or  {"type":"sky","animate":false}  or  {"type":"sky","follow_sun":true}
//                                                       or  {"type":"sky","follow_sun":true,"latitude":48.85,"longitude":2.35}
//    "overrides":[[58,255,255,255],[4,0,0,255]],           LED (0 to 145), red, green, blue
//    "level":1000}
//
// `speed` is thousandths of a turn per second (0 = fixed rainbow), `level` is 0 to 1000 as the brightness of the keyboard. An appearance with no
// overrides has an empty list. The file says nothing about whether the lighting is on: it is never switched on by a file.
inline constexpr qint64 MaxAppearanceBytes = 16384;

[[nodiscard]] QByteArray encodeAppearance(const galleon::keyboard::Appearance &appearance);
// nullopt for anything out of shape; `why`, when given, receives a short reason for the log (never the file's content).
[[nodiscard]] std::optional<galleon::keyboard::Appearance> parseAppearance(const QByteArray &bytes, QString *why = nullptr);

// ~/.config/galleoncore/lighting-appearance.json (the config folder of the platform).
[[nodiscard]] QString appearancePath();
// Atomic (written aside, then renamed). Creates the folder. False on failure, and nothing is written for an appearance that is not valid.
[[nodiscard]] bool writeAppearance(const QString &path, const galleon::keyboard::Appearance &appearance);
// Nothing for a missing, unreadable, oversized or invalid file.
[[nodiscard]] std::optional<galleon::keyboard::Appearance> readAppearance(const QString &path, QString *why = nullptr);

} // namespace galleond
