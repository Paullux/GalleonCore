// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "galleon/keyboard/BragiReply.h"

#include "galleon/keyboard/BragiFrame.h"

namespace galleon::keyboard {

namespace {

constexpr std::size_t MinPayloadForSet = 5; // 00 00 00 01 status
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

std::optional<PropertyReply> parsePropertyReply(std::span<const std::uint8_t> bytes)
{
    const auto payload = payloadOf(bytes);
    if (!payload)
        return std::nullopt;

    const std::uint8_t command = (*payload)[3];
    if (command == CommandSet) {
        if (payload->size() < MinPayloadForSet)
            return std::nullopt;
        return PropertyReply{.command = CommandSet, .status = (*payload)[4], .value = 0};
    }
    if (command == CommandGet) {
        if (payload->size() < MinPayloadForGet)
            return std::nullopt;
        PropertyReply reply{.command = CommandGet, .status = (*payload)[4], .value = 0};
        reply.value = static_cast<std::uint32_t>((*payload)[5]) | (static_cast<std::uint32_t>((*payload)[6]) << 8)
            | (static_cast<std::uint32_t>((*payload)[7]) << 16) | (static_cast<std::uint32_t>((*payload)[8]) << 24);
        return reply;
    }
    return std::nullopt;
}

} // namespace galleon::keyboard
