// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "KeySignalArgs.h"

#include <LocalControl.h>

#include <QString>

namespace keysignal {

// The request of the arguments, as galleond's local endpoint takes it (key-signal, key-signal-clear, key-signal-clear-all). Windows uses it; it is portable so that it is tested on Linux too.
[[nodiscard]] galleond::LocalCommand commandOf(const Arguments &arguments);

// Sends ONE command to the endpoint and waits for its "ok" (the frame was well formed; the endpoint says nothing about the lighting). "" when taken, else why not for a person to read.
[[nodiscard]] QString sendToEndpoint(const QString &endpoint, const galleond::LocalCommand &command, int timeoutMs = 1500);

} // namespace keysignal
