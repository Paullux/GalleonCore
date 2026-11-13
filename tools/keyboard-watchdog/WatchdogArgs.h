// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <string>

namespace watchdog {

struct Arguments
{
    std::uint32_t pid = 0;
    // `--dry-run`: wait for the process as usual, then say what would be done and stop, WITHOUT opening the keyboard.
    // It is what lets the installed program be tested with no keyboard (and never touch a real one).
    bool dryRun = false;
};

// What the watchdog is told: the process whose death it waits for. Exactly `--pid <N>`, optionally followed by
// `--dry-run`, with N a positive integer that fits 32 bits (a Windows process ID is 32 bits, a Linux one is at most
// 2^22). Anything else (other or more arguments, a sign, spaces, 0, a number too large) is refused: the watchdog
// sends nothing to the keyboard on arguments it does not understand.
[[nodiscard]] std::optional<Arguments> parseArguments(std::span<const std::string_view> arguments);

} // namespace watchdog
