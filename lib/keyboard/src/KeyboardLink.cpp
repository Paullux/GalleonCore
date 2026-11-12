// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "galleon/keyboard/KeyboardLink.h"

#include "galleon/keyboard/BragiFrame.h"
#include "galleon/keyboard/BragiReply.h"
#include "galleon/keyboard/Brightness.h"

#include <array>
#include <optional>
#include <thread>

namespace galleon::keyboard {

namespace {

constexpr std::size_t ReadBufferSize = 256;
constexpr int MaxStaleReports = 64; // bounded: a device that never stops talking cannot hold the caller
constexpr auto PollInterval = std::chrono::milliseconds(1);

// Reads and discards whatever is already waiting (an answer meant for someone else), then writes
// the request and waits for a reply whose echoed command matches. Never matches a stale answer.
std::optional<PropertyReply> exchange(Transport &transport, std::span<const std::uint8_t> request,
                                       std::uint8_t expectedCommand, std::chrono::milliseconds timeout,
                                       LinkError *failure)
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
        if (const auto reply = parsePropertyReply(received); reply && reply->command == expectedCommand)
            return reply;
    }
    return std::nullopt;
}

} // namespace

ReadResult readBrightness(Transport &transport, std::chrono::milliseconds timeout)
{
    ReadResult result;
    LinkError failure = LinkError::Timeout;
    const auto reply = exchange(transport, getPropertyRequest(PropertyBrightness), CommandGet, timeout, &failure);
    if (!reply) {
        result.error = failure;
        return result;
    }
    result.status = reply->status;
    if (reply->value > BrightnessMax) { // checked on the 32-bit value, before it is narrowed
        result.error = LinkError::InvalidReply;
        return result;
    }
    result.value = static_cast<std::uint16_t>(reply->value);
    return result;
}

WriteResult writeBrightness(Transport &transport, std::uint16_t value, std::chrono::milliseconds timeout)
{
    WriteResult result;
    if (!isValidBrightness(value)) {
        result.error = LinkError::InvalidValue;
        return result;
    }

    LinkError failure = LinkError::Timeout;
    const auto echo = exchange(transport, setPropertyRequest(PropertyBrightness, value), CommandSet, timeout, &failure);
    if (!echo) {
        result.error = failure;
        return result;
    }
    // The echo's status is kept for diagnostics but does not by itself decide success: the
    // read-back below is what confirms the value actually took (same spirit as
    // KeyboardReadProbe::readProperty, which keeps a non-zero status instead of failing on it).
    result.status = echo->status;

    const auto readBack = readBrightness(transport, timeout);
    if (readBack.error != LinkError::None) {
        result.error = readBack.error;
        return result;
    }
    result.confirmed = readBack.value;
    if (readBack.value != value) {
        result.error = LinkError::Unconfirmed;
        return result;
    }
    return result;
}

const char *describe(LinkError error)
{
    switch (error) {
    case LinkError::None: return "ok";
    case LinkError::InvalidValue: return "value outside 0..1000, nothing sent";
    case LinkError::WriteFailed: return "write of the request failed";
    case LinkError::Disconnected: return "device gone";
    case LinkError::Timeout: return "no answer in time";
    case LinkError::Unconfirmed: return "the read-back value does not match what was written";
    case LinkError::StaleReports: return "the keyboard kept sending old reports: nothing was sent";
    case LinkError::InvalidReply: return "the keyboard answered a brightness outside 0..1000";
    }
    return "unknown";
}

} // namespace galleon::keyboard
