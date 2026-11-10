// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "galleon/keyboard/Brightness.h"

#include <algorithm>

namespace galleon::keyboard {

std::uint16_t brightnessFromPercent(int percent)
{
    const int clamped = std::clamp(percent, 0, 100);
    return static_cast<std::uint16_t>((clamped * static_cast<int>(BrightnessMax) + 50) / 100);
}

int percentFromBrightness(std::uint16_t value)
{
    const std::uint16_t clamped = std::min(value, BrightnessMax);
    return (static_cast<int>(clamped) * 100 + static_cast<int>(BrightnessMax) / 2) / static_cast<int>(BrightnessMax);
}

} // namespace galleon::keyboard
