// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#include "WatchdogArgs.h"

#include <charconv>

namespace watchdog {

std::optional<std::uint32_t> parseArguments(std::span<const std::string_view> arguments)
{
    if (arguments.size() != 2 || arguments[0] != "--pid")
        return std::nullopt;
    const std::string_view text = arguments[1];
    if (text.empty() || text.front() < '0' || text.front() > '9') // no sign, no space
        return std::nullopt;
    std::uint64_t value = 0;
    const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
    if (result.ec != std::errc{} || result.ptr != text.data() + text.size() || value == 0 || value > 0xffffffffULL)
        return std::nullopt;
    return static_cast<std::uint32_t>(value);
}

} // namespace watchdog
