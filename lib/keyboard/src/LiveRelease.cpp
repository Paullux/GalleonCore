// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "galleon/keyboard/LiveRelease.h"

#include "galleon/keyboard/BragiReply.h"
#include "galleon/keyboard/LiveCommandGuard.h"
#include "galleon/keyboard/LiveRequests.h"
#include "galleon/keyboard/LiveYield.h"

#include "Exchange.h"

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

// The 128 payload bytes of an answer, whichever way the transport framed it (as BragiReply does).
std::optional<std::span<const std::uint8_t>> payloadOf(std::span<const std::uint8_t> bytes)
{
    if (bytes.size() == PayloadSize)
        return bytes;
    if (bytes.size() == WriteSize && bytes.front() == 0)
        return bytes.subspan(1);
    return std::nullopt;
}

// One checked exchange: the report goes through the allowlist first, the answer must echo the command, and the
// guard moves on only when the keyboard answered with status 0.
class Checked
{
public:
    Checked(Transport &transport, std::chrono::milliseconds timeout) : m_transport(transport), m_timeout(timeout) {}

    // The 128-byte answer, or nothing (lastError says why).
    std::optional<std::vector<std::uint8_t>> send(const Request &request)
    {
        if (m_guard.check(request) != Refusal::None) {
            m_error = LinkError::WriteFailed; // never sent
            return std::nullopt;
        }
        const std::uint8_t command = request[4];
        const auto accept = [command](std::span<const std::uint8_t> received) {
            const auto payload = payloadOf(received);
            return payload && (*payload)[3] == command;
        };
        auto answer = detail::exchange(m_transport, request, accept, m_timeout, &m_error);
        if (!answer)
            return std::nullopt;
        const auto payload = payloadOf(*answer);
        if (!payload) {
            m_error = LinkError::InvalidReply;
            return std::nullopt;
        }
        if ((*payload)[4] != 0) { // a non-zero status is a failure
            m_error = LinkError::InvalidReply;
            return std::nullopt;
        }
        m_guard.confirmed(request);
        return std::vector<std::uint8_t>(payload->begin(), payload->end());
    }

    [[nodiscard]] LinkError error() const { return m_error; }

private:
    Transport &m_transport;
    std::chrono::milliseconds m_timeout;
    LiveCommandGuard m_guard;
    LinkError m_error = LinkError::None;
};

// Session start: the answer repeats the 4 random bytes and ends with the session ID (docs/protocol/README.md,
// command log: `1b 00 rr rr rr rr 01`). Session 0 means none.
std::optional<std::uint8_t> startSession(Checked &link, const std::array<std::uint8_t, 4> &random)
{
    const auto answer = link.send(sessionStartRequest(random));
    if (!answer)
        return std::nullopt;
    const std::uint8_t session = (*answer)[PayloadAt + 4];
    return session == 0 ? std::nullopt : std::optional<std::uint8_t>(session);
}

std::optional<std::uint8_t> readMode(Checked &link, std::uint8_t session)
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

        Checked link(transport, timeout);
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
