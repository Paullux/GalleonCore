// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

namespace galleond {

class LocalControl;
class KeySignalsService;

// The way in of other programs on Windows: the actions `key-signal`, `key-signal-clear` and `key-signal-clear-all` of the local endpoint (per-user named pipe, no
// network) go to the same KeySignalsService as the D-Bus object of Linux, so the opt-in of the person, the limits and the cleanup are the same. Kept apart from
// main.cpp so that it is the same code on both systems and tested on Linux. There is no reply on this endpoint: nothing here returns anything to the program.
// Both must outlive each other's signals; main.cpp builds the service first.
void connectKeySignals(LocalControl &control, KeySignalsService &service);

} // namespace galleond
