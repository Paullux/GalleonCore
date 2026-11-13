// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "galleon/keyboard/KeyboardLink.h"

#include <galleon/Transport.h>

#include <array>
#include <chrono>
#include <cstdint>
#include <optional>

// What the watchdog does once the process that wrote the live frames is gone, over a galleon::Transport: open a
// fresh session, read the operating mode (property 3), and, only if the keyboard is still in host mode (2), give
// the keys back. From docs/WINDOWS_LIVE_LIGHTING_CSHARP.md, "Release is the host's job" (Aaryush1, issue #56):
// the keyboard stays in host mode after a crash with no timeout, so a second process releases it.
//
// Release sequence: unbind, set the mode to 1, read the mode back to confirm; each step is attempted even if the
// one before failed. Up to MaxAttempts tries, then it gives up and says so (it never loops forever).
//
// Everything that is sent first passes LiveCommandGuard: a report the allowlist refuses is not sent and counts as
// a failure of that step. Nothing here writes a frame, opens a file, or touches any property but the mode, and
// nothing here yields to another host: the watchdog acts only on a keyboard still in mode 2, and a mode of 1
// (the writer released already, or another host holds it) is left alone.
namespace galleon::keyboard {

enum class ReleaseOutcome : std::uint8_t {
    NotNeeded, // the mode was read and is not 2: nothing was changed
    Released,  // the keyboard was in mode 2 and now reads back 1
    Failed     // gave up: the mode could not be read, or the release was not confirmed, after every attempt
};

struct ReleaseResult
{
    ReleaseOutcome outcome = ReleaseOutcome::Failed;
    std::optional<std::uint8_t> modeBefore; // the mode as first read, when it could be read
    int attempts = 0;
    LinkError lastError = LinkError::None;
};

inline constexpr int MaxReleaseAttempts = 3;

// `random` are the 4 random bytes of the session start (the caller draws them). `retryDelay` is the wait between
// two attempts (0 in tests).
[[nodiscard]] ReleaseResult releaseIfHostMode(Transport &transport, const std::array<std::uint8_t, 4> &random,
                                               std::chrono::milliseconds timeout, std::chrono::milliseconds retryDelay);

[[nodiscard]] const char *describe(ReleaseOutcome outcome);

} // namespace galleon::keyboard
