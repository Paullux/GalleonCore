// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QByteArray>
#include <QString>

#include <optional>

namespace galleond {

// The one shape of the files by which the person allows something that is off by default, written by Bosun only and read by galleond (AGENTS.md, security and
// privacy by design, rules 5 and 6: explicit activation, and a program on the bus can never switch it on). Used by key-signals.json (other programs may light
// keys) and key-glow.json (the keys glow under the fingers):
//
//   {"version":1,"allow":true}
//
// Like every file read from disk it is untrusted: a bounded size, exactly these two keys, a true or false, or nothing at all (which means off).
inline constexpr qint64 MaxAllowFileBytes = 256;

[[nodiscard]] QByteArray encodeAllowFile(bool allow);
[[nodiscard]] std::optional<bool> parseAllowFile(const QByteArray &bytes, QString *why = nullptr);
// Atomic (written aside, then renamed). False on failure.
[[nodiscard]] bool writeAllowFile(const QString &path, bool allow);
// Nothing for a missing, unreadable, oversized or invalid file.
[[nodiscard]] std::optional<bool> readAllowFile(const QString &path, QString *why = nullptr);
// What a daemon acts on: true only for a file that says so.
[[nodiscard]] bool allowFileSaysYes(const QString &path);

} // namespace galleond
