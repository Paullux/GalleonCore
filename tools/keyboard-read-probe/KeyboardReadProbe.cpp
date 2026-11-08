// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#include "KeyboardReadProbe.h"

#include <algorithm>
#include <thread>

namespace keyboardprobe {

namespace {

constexpr std::size_t ReadBufferSize = 256;
constexpr int MaxStaleReports = 64; // bounded: a device that never stops talking cannot hold the probe
constexpr auto PollInterval = std::chrono::milliseconds(1);
constexpr std::size_t MinPayloadForGet = 9; // 00 00 00 02 status + 4 value bytes

// The 128 payload bytes of an answer, whichever way the transport framed it.
std::optional<std::span<const std::uint8_t>> payloadOf(std::span<const std::uint8_t> bytes)
{
    if (bytes.size() == PayloadSize)
        return bytes;
    if (bytes.size() == WriteSize && bytes.front() == 0)
        return bytes.subspan(1);
    return std::nullopt;
}

} // namespace

bool isReadableProperty(std::uint8_t property)
{
    return std::ranges::find(ReadableProperties, property) != ReadableProperties.end();
}

std::array<std::uint8_t, WriteSize> getPropertyRequest(std::uint8_t property)
{
    std::array<std::uint8_t, WriteSize> request{};
    request[2] = 0x01;
    request[4] = CommandGet;
    request[5] = property;
    return request;
}

std::optional<PropertyReply> parsePropertyReply(std::span<const std::uint8_t> bytes)
{
    const auto payload = payloadOf(bytes);
    if (!payload || payload->size() < MinPayloadForGet || (*payload)[3] != CommandGet)
        return std::nullopt;
    PropertyReply reply;
    reply.status = (*payload)[4];
    reply.value = static_cast<std::uint32_t>((*payload)[5]) | (static_cast<std::uint32_t>((*payload)[6]) << 8)
        | (static_cast<std::uint32_t>((*payload)[7]) << 16) | (static_cast<std::uint32_t>((*payload)[8]) << 24);
    return reply;
}

ReadResult readProperty(galleon::Transport &transport, std::uint8_t property, std::chrono::milliseconds timeout)
{
    ReadResult result;
    if (!isReadableProperty(property)) {
        result.error = ReadError::NotAllowed;
        return result;
    }

    std::array<std::uint8_t, ReadBufferSize> buffer{};
    // Never match a stale answer (for instance one meant for the Web Hub).
    for (int i = 0; i < MaxStaleReports; ++i) {
        const int n = transport.read(buffer);
        if (n < 0) {
            result.error = ReadError::Disconnected;
            return result;
        }
        if (n == 0)
            break;
        ++result.staleDiscarded;
    }

    if (!transport.write(getPropertyRequest(property))) {
        result.error = ReadError::WriteFailed;
        return result;
    }

    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        const int n = transport.read(buffer);
        if (n < 0) {
            result.error = ReadError::Disconnected;
            return result;
        }
        if (n == 0) {
            std::this_thread::sleep_for(PollInterval);
            continue;
        }
        const auto received = std::span<const std::uint8_t>(buffer).first(static_cast<std::size_t>(n));
        if (const auto reply = parsePropertyReply(received)) {
            result.reply = *reply;
            result.raw.assign(received.begin(), received.end());
            return result;
        }
        ++result.unexpected;
    }
    result.error = ReadError::Timeout;
    return result;
}

const char *describe(ReadError error)
{
    switch (error) {
    case ReadError::None: return "ok";
    case ReadError::NotAllowed: return "property not on the read-only list";
    case ReadError::WriteFailed: return "write of the request failed";
    case ReadError::Disconnected: return "device gone";
    case ReadError::Timeout: return "no answer in time";
    }
    return "unknown";
}

} // namespace keyboardprobe
