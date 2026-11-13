// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "galleon/keyboard/KeyboardLink.h"

#include <galleon/Transport.h>

#include <array>
#include <chrono>
#include <functional>
#include <optional>
#include <span>
#include <vector>
#include <cstddef>
#include <cstdint>

// Internal to lib/keyboard (not installed): one request, one answer, over a galleon::Transport. Shared by
// KeyboardLink (brightness) and LiveRelease (the watchdog's release) so the stale-answer discipline lives in
// one place. It reads and discards whatever already waits (an answer meant for someone else), sends the request,
// and returns the first report `accept` approves. It never matches a stale answer.
namespace galleon::keyboard::detail {

inline constexpr std::size_t ReadBufferSize = 256;
inline constexpr int MaxStaleReports = 64; // bounded: a device that never stops talking cannot hold the caller

// The report as read, or nothing; `failure` says why (Timeout when nothing acceptable arrived in time).
[[nodiscard]] std::optional<std::vector<std::uint8_t>> exchange(Transport &transport, std::span<const std::uint8_t> request,
                                                                 const std::function<bool(std::span<const std::uint8_t>)> &accept,
                                                                 std::chrono::milliseconds timeout, LinkError *failure);

} // namespace galleon::keyboard::detail
