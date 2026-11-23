// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

namespace galleond {

// True while Windows is shutting down or ending the session of the person (GetSystemMetrics(SM_SHUTTINGDOWN)); always false on the other systems, where
// the watchdog is not stopped by the end of the session (LightingController::setSessionEndingCheck says why it matters).
[[nodiscard]] bool sessionIsEnding();

} // namespace galleond
