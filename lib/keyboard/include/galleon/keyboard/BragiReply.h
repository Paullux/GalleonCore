// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <cstdint>
#include <optional>
#include <span>

// Pure validation of a Bragi answer (see BragiFrame.h for the request side). Independent of
// Protocol.h/StreamDeck.h: see AGENTS.md "Maintaining architecture over time".
namespace galleon::keyboard {

struct PropertyReply
{
    std::uint8_t command = 0; // CommandGet or CommandSet, echoed back
    std::uint8_t status = 0;
    std::uint32_t value = 0; // only meaningful for a CommandGet reply; 0 for a CommandSet echo
};

// Accepts 128 bytes (unnumbered report, as hidapi/hidraw return it) or 129 bytes with a leading
// 00 (report ID). A get answer needs the 4 value bytes; a set answer is an echo with no value and
// a shorter minimum length. Anything else, another command, or a truncated answer gives nothing.
[[nodiscard]] std::optional<PropertyReply> parsePropertyReply(std::span<const std::uint8_t> bytes);

} // namespace galleon::keyboard
