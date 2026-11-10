// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <cstdint>

// Pure range and conversion helpers for keyboard property 0xEA (brightness), from
// docs/protocol/README.md "Keyboard brightness: property 0xEA" (2026-11-08): 0 to 1000, kept
// across a power cycle. The Web Hub only ever sends six steps of 200, but nothing here has
// observed the device reject another value in range, so any 0..1000 value is accepted.
namespace galleon::keyboard {

inline constexpr std::uint8_t PropertyBrightness = 0xea;
inline constexpr std::uint16_t BrightnessMin = 0;
inline constexpr std::uint16_t BrightnessMax = 1000;

[[nodiscard]] constexpr bool isValidBrightness(std::uint32_t value)
{
    return value <= BrightnessMax;
}

// Clamps an out-of-range percentage instead of producing an invalid brightness value.
[[nodiscard]] std::uint16_t brightnessFromPercent(int percent);

// Rounds to the nearest percent; an out-of-range value (isValidBrightness false) is clamped first.
[[nodiscard]] int percentFromBrightness(std::uint16_t value);

} // namespace galleon::keyboard
