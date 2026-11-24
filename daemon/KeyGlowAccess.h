// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <galleon/keyboard/Appearance.h>
#include <galleon/keyboard/ReactiveGlow.h>

#include <QByteArray>
#include <QString>

#include <optional>

namespace galleond {

// The choice "the keys glow under the fingers" (key-glow.json): whether it is on, and in which colour. The glow reads the key presses of the keyboard, so it is
// OFF unless the person made the choice, in Bosun, where its state is always shown (AGENTS.md, security and privacy by design, rule 5). One writer: Bosun writes
// it, galleond only reads it, and no program on the bus or the local endpoint can change it.
//
//   {"version":1,"allow":true,"colour":"#00c8ff"}
//
// Like every file read from disk it is untrusted: a bounded size, exactly these three keys, a true or false, a colour written as # and six hexadecimal digits,
// or nothing at all. A file that is missing, oversized or damaged means OFF (and the default colour): nothing is guessed from a part of it.
inline constexpr qint64 MaxKeyGlowAccessBytes = 256;

struct KeyGlowChoice
{
    bool allow = false;
    galleon::keyboard::Rgb colour = galleon::keyboard::DefaultGlowColour;

    friend bool operator==(const KeyGlowChoice &, const KeyGlowChoice &) = default;
};

// "#rrggbb" for a colour (lower case), and back; nothing for anything that is not exactly that.
[[nodiscard]] QString keyGlowColourText(galleon::keyboard::Rgb colour);
[[nodiscard]] std::optional<galleon::keyboard::Rgb> parseKeyGlowColour(const QString &text);

[[nodiscard]] QByteArray encodeKeyGlowChoice(const KeyGlowChoice &choice);
[[nodiscard]] std::optional<KeyGlowChoice> parseKeyGlowChoice(const QByteArray &bytes, QString *why = nullptr);
// ~/.config/galleoncore/key-glow.json (the config folder of the platform).
[[nodiscard]] QString keyGlowAccessPath();
// Atomic (written aside, then renamed). False on failure.
[[nodiscard]] bool writeKeyGlowChoice(const QString &path, const KeyGlowChoice &choice);
// Nothing for a missing, unreadable, oversized or invalid file.
[[nodiscard]] std::optional<KeyGlowChoice> readKeyGlowChoice(const QString &path, QString *why = nullptr);
// What the daemon acts on: the choice of a good file, else off with the default colour.
[[nodiscard]] KeyGlowChoice keyGlowChoice(const QString &path);
// Whether the glow is on, from a good file; false for anything else.
[[nodiscard]] inline bool keyGlowAllowed(const QString &path) { return keyGlowChoice(path).allow; }

} // namespace galleond
