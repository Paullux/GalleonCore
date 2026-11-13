// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>

namespace watchdog {

enum class WaitResult : std::uint8_t {
    Exited,      // the process was running and has ended
    AlreadyGone, // there was no such process when the watch began
    CannotWatch  // the process could not be watched (rights, or no means on this system): nothing may be released
};

// Blocks until the process with this ID ends. The process is held by a handle (a pidfd on Linux, a process
// handle on Windows) taken before waiting, so an ID reused by another process after the first one ended is never
// mistaken for it. On Linux without pidfd support (kernel before 5.3) it gives CannotWatch rather than poll an ID
// that could be reused: the watchdog would then do nothing instead of releasing under a live writer.
[[nodiscard]] WaitResult waitForExit(std::uint32_t pid);

} // namespace watchdog
