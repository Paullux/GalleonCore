// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "Exchange.h"

#include <thread>

namespace galleon::keyboard::detail {

namespace {
constexpr auto PollInterval = std::chrono::milliseconds(1);
}

std::optional<std::vector<std::uint8_t>> exchange(Transport &transport, std::span<const std::uint8_t> request,
                                                   const std::function<bool(std::span<const std::uint8_t>)> &accept,
                                                   std::chrono::milliseconds timeout, LinkError *failure)
{
    std::array<std::uint8_t, ReadBufferSize> buffer{};
    bool drained = false;
    for (int i = 0; i < MaxStaleReports && !drained; ++i) {
        const int n = transport.read(buffer);
        if (n < 0) {
            *failure = LinkError::Disconnected;
            return std::nullopt;
        }
        drained = n == 0;
    }
    if (!drained) { // never saw an empty queue: the next report could be an old answer, so send nothing
        *failure = LinkError::StaleReports;
        return std::nullopt;
    }

    if (!transport.write(request)) {
        *failure = LinkError::WriteFailed;
        return std::nullopt;
    }

    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        const int n = transport.read(buffer);
        if (n < 0) {
            *failure = LinkError::Disconnected;
            return std::nullopt;
        }
        if (n == 0) {
            std::this_thread::sleep_for(PollInterval);
            continue;
        }
        const auto received = std::span<const std::uint8_t>(buffer).first(static_cast<std::size_t>(n));
        if (accept(received))
            return std::vector<std::uint8_t>(received.begin(), received.end());
    }
    *failure = LinkError::Timeout;
    return std::nullopt;
}

} // namespace galleon::keyboard::detail
