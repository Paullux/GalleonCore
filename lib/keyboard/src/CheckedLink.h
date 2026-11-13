// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "Exchange.h"

#include "galleon/keyboard/LiveCommandGuard.h"
#include "galleon/keyboard/LiveRequests.h"

#include <chrono>
#include <cstdint>
#include <optional>
#include <vector>

// Internal to lib/keyboard (not installed). The one way a live-lighting request leaves this library: through the
// allowlist first (LiveCommandGuard: a refused report is never sent), then one exchange whose answer must echo the
// command and carry status 0. The guard moves on only for an answer of status 0, so it never believes in a state the
// keyboard is not in. Used by the watchdog's release (LiveRelease) and by the live link (LiveLink).
namespace galleon::keyboard::detail {

// The 128 payload bytes of an answer, whichever way the transport framed it.
[[nodiscard]] std::optional<std::span<const std::uint8_t>> answerPayload(std::span<const std::uint8_t> bytes);

class CheckedLink
{
public:
    CheckedLink(Transport &transport, std::chrono::milliseconds timeout, ReportObserver unmatched = {});

    // The 128-byte answer, or nothing (error() says why). A refused report is not sent: WriteFailed.
    [[nodiscard]] std::optional<std::vector<std::uint8_t>> send(const Request &request);

    [[nodiscard]] LinkError error() const { return m_error; }
    [[nodiscard]] LiveCommandGuard &guard() { return m_guard; }
    [[nodiscard]] const LiveCommandGuard &guard() const { return m_guard; }

private:
    Transport &m_transport;
    std::chrono::milliseconds m_timeout;
    ReportObserver m_unmatched;
    LiveCommandGuard m_guard;
    LinkError m_error = LinkError::None;
};

} // namespace galleon::keyboard::detail
