// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "AllowFile.h"

#include <QByteArray>
#include <QString>

#include <optional>

namespace galleond {

// The choice "other programs may light single keys" (key-signals.json): OFF unless the person made it, in Bosun, where it is shown. One writer: Bosun writes it,
// galleond only reads it, so that a program on the bus can never switch it on by itself (AGENTS.md, security and privacy by design, rules 5 and 6). The shape of
// the file, and how it is read as untrusted, are those of AllowFile.h.
inline constexpr qint64 MaxKeySignalsAccessBytes = MaxAllowFileBytes;

[[nodiscard]] inline QByteArray encodeKeySignalsAccess(bool allow) { return encodeAllowFile(allow); }
[[nodiscard]] inline std::optional<bool> parseKeySignalsAccess(const QByteArray &bytes, QString *why = nullptr) { return parseAllowFile(bytes, why); }
// ~/.config/galleoncore/key-signals.json (the config folder of the platform).
[[nodiscard]] QString keySignalsAccessPath();
[[nodiscard]] inline bool writeKeySignalsAccess(const QString &path, bool allow) { return writeAllowFile(path, allow); }
[[nodiscard]] inline std::optional<bool> readKeySignalsAccess(const QString &path, QString *why = nullptr) { return readAllowFile(path, why); }
// What the daemon acts on: true only for a file that says so.
[[nodiscard]] inline bool keySignalsAllowed(const QString &path) { return allowFileSaysYes(path); }

} // namespace galleond
