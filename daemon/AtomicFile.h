// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QByteArray>
#include <QFileDevice>
#include <QString>

#include <functional>
#include <optional>

namespace galleond {

// Writing a small file that another program watches and reads (the brightness curve, keyboard-state.json,
// lighting-state.json): written aside and renamed over the old one, so a reader never sees half a file.
//
// On Windows the rename is refused for as long as ANY program has the old file open without allowing it to be replaced,
// and a program that reads it just after a change notification does exactly that, for a moment. Two saves in a row
// (dragging a point of the curve) then lose the second one now and then. So a refused save is tried again, a few times,
// a few milliseconds apart, before it is reported as failed. The total wait is bounded (MaxAttempts * RetryDelayMs), because
// this runs on threads that also have other work (the keep-alive of the Stream Deck).
inline constexpr int MaxAttempts = 6;
inline constexpr int RetryDelayMs = 10;

// Pure: runs `once` up to `attempts` times and calls `wait(attempt)` between two tries (never after the last, never after
// a success). True as soon as one try succeeds.
[[nodiscard]] bool retryUntilDone(int attempts, const std::function<bool()> &once, const std::function<void(int)> &wait);

// Creates the folder, then writes `bytes` to `path` as above. False when every try failed. `permissions` are those of the
// new file when given (the pages file keeps those of the one it replaces); `error`, when given, receives the reason of the
// last failure.
[[nodiscard]] bool writeFileAtomically(const QString &path, const QByteArray &bytes,
                                       std::optional<QFileDevice::Permissions> permissions = std::nullopt, QString *error = nullptr);

} // namespace galleond
