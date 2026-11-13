// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "galleon/keyboard/LiveRelease.h"

#include "galleon/keyboard/BragiReply.h"
#include "galleon/keyboard/LiveCommandGuard.h"
#include "galleon/keyboard/LiveRequests.h"
#include "galleon/keyboard/LiveYield.h"

#include "CheckedLink.h"

#include <thread>
#include <vector>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace galleon::keyboard {

namespace {

constexpr std::size_t PayloadAt = 5;

using detail::CheckedLink;

// Session start: the answer repeats the 4 random bytes and ends with the session ID (docs/protocol/README.md,
// command log: `1b 00 rr rr rr rr 01`). Session 0 means none.
std::optional<std::uint8_t> startSession(CheckedLink &link, const std::array<std::uint8_t, 4> &random)
{
    const auto answer = link.send(sessionStartRequest(random));
    if (!answer)
        return std::nullopt;
    const std::uint8_t session = (*answer)[PayloadAt + 4];
    return session == 0 ? std::nullopt : std::optional<std::uint8_t>(session);
}

std::optional<std::uint8_t> readMode(CheckedLink &link, std::uint8_t session)
{
    const auto answer = link.send(getPropertyIn(session, PropertyMode));
    if (!answer)
        return std::nullopt;
    const auto parsed = parsePropertyReply(*answer);
    if (!parsed || parsed->command != CommandGet || parsed->value > 0xff)
        return std::nullopt;
    return static_cast<std::uint8_t>(parsed->value);
}

} // namespace

ReleaseResult releaseIfHostMode(Transport &transport, const std::array<std::uint8_t, 4> &random,
                                std::chrono::milliseconds timeout, std::chrono::milliseconds retryDelay)
{
    ReleaseResult result;
    bool triedRelease = false; // a release step was sent in an earlier attempt
    for (int attempt = 1; attempt <= MaxReleaseAttempts; ++attempt) {
        result.attempts = attempt;
        if (attempt > 1)
            std::this_thread::sleep_for(retryDelay);

        CheckedLink link(transport, timeout);
        const auto session = startSession(link, random);
        if (!session) {
            result.lastError = link.error();
            continue;
        }
        const auto mode = readMode(link, *session);
        if (!mode) {
            result.lastError = link.error();
            continue;
        }
        if (!result.modeBefore)
            result.modeBefore = mode;
        if (!watchdogShouldRelease(mode)) {
            // Reading a 1 after an attempt that sent the release means that attempt did work, unconfirmed.
            result.outcome = triedRelease && *mode == ModeSelfOperated ? ReleaseOutcome::Released : ReleaseOutcome::NotNeeded;
            result.lastError = LinkError::None;
            return result;
        }

        // Release: unbind, mode 1, read back. Each step is tried even if the previous one failed.
        triedRelease = true;
        const bool unbound = link.send(unbindRequest(*session)).has_value();
        const bool set = link.send(setModeRequest(*session, ModeSelfOperated)).has_value();
        const auto after = readMode(link, *session);
        if (after == ModeSelfOperated) {
            result.outcome = ReleaseOutcome::Released;
            result.lastError = LinkError::None;
            return result;
        }
        result.lastError = after ? LinkError::Unconfirmed : link.error();
        static_cast<void>(unbound);
        static_cast<void>(set);
    }
    result.outcome = ReleaseOutcome::Failed;
    return result;
}

const char *describe(ReleaseOutcome outcome)
{
    switch (outcome) {
    case ReleaseOutcome::NotNeeded: return "not in host mode: nothing changed";
    case ReleaseOutcome::Released: return "released: the keyboard is back in its own mode";
    case ReleaseOutcome::Failed: return "gave up: the release could not be confirmed";
    }
    return "unknown";
}

} // namespace galleon::keyboard
