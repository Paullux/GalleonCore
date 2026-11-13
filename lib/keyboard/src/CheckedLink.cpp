// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "CheckedLink.h"

namespace galleon::keyboard::detail {

namespace {
constexpr std::size_t DirectionAt = 1; // 15 here means the session expired (docs/protocol/README.md, frame header)
constexpr std::size_t CommandAt = 3;
constexpr std::size_t StatusAt = 4;
constexpr std::uint8_t SessionExpiredMarker = 15;
} // namespace

std::optional<std::span<const std::uint8_t>> answerPayload(std::span<const std::uint8_t> bytes)
{
    if (bytes.size() == PayloadSize)
        return bytes;
    if (bytes.size() == WriteSize && bytes.front() == 0)
        return bytes.subspan(1);
    return std::nullopt;
}

CheckedLink::CheckedLink(Transport &transport, std::chrono::milliseconds timeout, ReportObserver unmatched)
    : m_transport(transport), m_timeout(timeout), m_unmatched(std::move(unmatched))
{
}

std::optional<std::vector<std::uint8_t>> CheckedLink::send(const Request &request)
{
    if (m_guard.check(request) != Refusal::None) {
        m_error = LinkError::WriteFailed; // never sent
        return std::nullopt;
    }
    const std::uint8_t command = request[4];
    const auto accept = [command](std::span<const std::uint8_t> received) {
        const auto payload = answerPayload(received);
        return payload && (*payload)[CommandAt] == command;
    };
    auto answer = exchange(m_transport, request, accept, m_timeout, &m_error, m_unmatched);
    if (!answer)
        return std::nullopt;
    const auto payload = answerPayload(*answer);
    if (!payload) {
        m_error = LinkError::InvalidReply;
        return std::nullopt;
    }
    if ((*payload)[DirectionAt] == SessionExpiredMarker) {
        m_error = LinkError::SessionExpired;
        return std::nullopt;
    }
    if ((*payload)[StatusAt] != 0) { // a non-zero status is a failure
        m_error = LinkError::InvalidReply;
        return std::nullopt;
    }
    m_guard.confirmed(request);
    return std::vector<std::uint8_t>(payload->begin(), payload->end());
}

} // namespace galleon::keyboard::detail
