// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "galleon/keyboard/KeyboardLink.h"

#include "galleon/keyboard/BragiFrame.h"
#include "galleon/keyboard/BragiReply.h"
#include "galleon/keyboard/Brightness.h"

#include "Exchange.h"

#include <optional>

namespace galleon::keyboard {

namespace {

// One request and its answer, through the shared exchange (Exchange.h): the answer is the first report that parses
// and echoes the expected command, so a stale answer or a malformed one is never taken for it.
std::optional<PropertyReply> exchange(Transport &transport, std::span<const std::uint8_t> request,
                                       std::uint8_t expectedCommand, std::chrono::milliseconds timeout,
                                       LinkError *failure)
{
    std::optional<PropertyReply> reply;
    const auto accept = [&reply, expectedCommand](std::span<const std::uint8_t> received) {
        const auto parsed = parsePropertyReply(received);
        if (!parsed || parsed->command != expectedCommand)
            return false;
        reply = parsed;
        return true;
    };
    if (!detail::exchange(transport, request, accept, timeout, failure))
        return std::nullopt;
    return reply;
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
    case LinkError::SessionExpired: return "the keyboard ended the session";
    }
    return "unknown";
}

} // namespace galleon::keyboard
