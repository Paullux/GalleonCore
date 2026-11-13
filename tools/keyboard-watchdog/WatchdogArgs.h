// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <string>

namespace watchdog {

// The one thing the watchdog is told: the process whose death it waits for. Exactly `--pid <N>` with N a
// positive integer that fits 32 bits (a Windows process ID is 32 bits, a Linux one is at most 2^22). Anything
// else (more or fewer arguments, a sign, spaces, 0, a number too large) is refused: the watchdog sends nothing
// to the keyboard on arguments it does not understand.
[[nodiscard]] std::optional<std::uint32_t> parseArguments(std::span<const std::string_view> arguments);

} // namespace watchdog
