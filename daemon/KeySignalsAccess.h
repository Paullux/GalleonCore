// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QByteArray>
#include <QString>

#include <optional>

namespace galleond {

// The choice "other programs may light single keys" (key-signals.json): OFF unless the person made it, in Bosun, where it is shown. One writer: Bosun writes it,
// galleond only reads it, so that a program on the bus can never switch it on by itself (AGENTS.md, security and privacy by design, rules 5 and 6).
//
//   {"version":1,"allow":true}
//
// Like every file read from disk it is untrusted: a bounded size, exactly these two keys, a true or false, or nothing at all (which means off).
inline constexpr qint64 MaxKeySignalsAccessBytes = 256;

[[nodiscard]] QByteArray encodeKeySignalsAccess(bool allow);
[[nodiscard]] std::optional<bool> parseKeySignalsAccess(const QByteArray &bytes, QString *why = nullptr);
// ~/.config/galleoncore/key-signals.json (the config folder of the platform).
[[nodiscard]] QString keySignalsAccessPath();
// Atomic (written aside, then renamed). False on failure.
[[nodiscard]] bool writeKeySignalsAccess(const QString &path, bool allow);
// Nothing for a missing, unreadable, oversized or invalid file.
[[nodiscard]] std::optional<bool> readKeySignalsAccess(const QString &path, QString *why = nullptr);
// What the daemon acts on: true only for a file that says so.
[[nodiscard]] bool keySignalsAllowed(const QString &path);

} // namespace galleond
