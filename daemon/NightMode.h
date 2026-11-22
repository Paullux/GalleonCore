// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QByteArray>
#include <QDateTime>
#include <QString>

#include <optional>

namespace galleond {

// "Lights off until morning" for the module and the dial rings: the module goes dark (brightness 0, black screen and keys) and the rings are out
// until an end time, whatever else wants to draw. Asked for by Aaryush1 on #111 (his keyboard helper has the same mode); decided by the owner.
// This file is the pure part: the end time, and the one small file that makes a restart of galleond keep the night going.
//
//   night-mode.json  {"version":1,"until":1783938123}      written by galleond ONLY, present only while the night lasts
//
// `until` is the end, in seconds since 1970. The file is read as untrusted input: a bounded size, exactly the keys above, and an end that is in
// the future and no more than MaxNightSeconds away (a damaged, expired or absurd file means "no night", so it can never keep the module dark for good).
// It says nothing about keys, dials or anything the person typed.
inline constexpr qint64 MaxNightSeconds = qint64{24} * 60 * 60;
inline constexpr qint64 MaxNightFileBytes = 256;
// The brightness the module is given back at the end of the night, in percent. The module's own setting before the night is not known to galleond
// (it never changes it otherwise), so this is the full brightness: a value to measure, see ROADMAP.md.
inline constexpr int NightDayBrightness = 100;

// The next HH:MM strictly after `now`, in the time zone of `now` (a night that starts at 07:30 with an end of 07:00 ends the next day at 07:00; an end
// equal to the time now is a whole day away). Invalid for an hour outside 0 to 23 or a minute outside 0 to 59.
[[nodiscard]] QDateTime nightEnd(const QDateTime &now, int hour, int minute);

[[nodiscard]] QByteArray encodeNightUntil(qint64 until);
// nullopt for anything out of shape, or an end that is not in (now, now + MaxNightSeconds]. `why`, when given, receives a short reason for the log.
[[nodiscard]] std::optional<qint64> parseNightUntil(const QByteArray &bytes, qint64 now, QString *why = nullptr);

// ~/.config/galleoncore/night-mode.json (the config folder of the platform).
[[nodiscard]] QString nightModePath();
// Atomic (written aside, then renamed). False on failure.
[[nodiscard]] bool writeNightUntil(const QString &path, qint64 until);
// Nothing for a missing, unreadable, oversized or invalid file, or an end that is already past or too far.
[[nodiscard]] std::optional<qint64> readNightUntil(const QString &path, qint64 now, QString *why = nullptr);
// Removes the file (the night is over). True when there is no file afterwards.
[[nodiscard]] bool removeNightFile(const QString &path);

} // namespace galleond
