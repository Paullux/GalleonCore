// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "KeySignalArgs.h"

#include <LocalControl.h>

#include <QList>
#include <QString>

namespace keysignal {

// The requests of the arguments, as galleond's local endpoint takes them (key-signal, key-signal-clear, key-signal-clear-all): one for each key, in the order they were named, or the
// single clear-all. Windows uses it; it is portable so that it is tested on Linux too.
[[nodiscard]] QList<galleond::LocalCommand> commandsOf(const Arguments &arguments);

// Sends ONE command to the endpoint and waits for its "ok" (the frame was well formed; the endpoint says nothing about the lighting). "" when taken, else why not for a person to read.
[[nodiscard]] QString sendToEndpoint(const QString &endpoint, const galleond::LocalCommand &command, int timeoutMs = 1500);

// Sends the commands one after the other, each on its own connection, and stops at the first that fails. "" when all were taken; else why not, naming the key of the one that failed
// and how many were sent before it (they stay in force: the endpoint has no way to take a call back, and every signal ends by itself).
[[nodiscard]] QString sendAllToEndpoint(const QString &endpoint, const QList<galleond::LocalCommand> &commands, int timeoutMs = 1500);

} // namespace keysignal
