// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "galleon/keyboard/KeyReport.h"

#include <algorithm>

namespace galleon::keyboard {
namespace {

constexpr std::size_t ModifierByte = 1;
constexpr std::size_t BitmapFirstByte = 2;
constexpr std::size_t InternationalByte = 17;
constexpr HidUsage FirstModifier = 0xE0;
constexpr HidUsage FirstUsageThatIsAKey = 4; // 0 is "no event", 1 to 3 are the rollover and POST-fail codes
constexpr HidUsage LastBitmapUsage = 0x73;   // the bitmap has 120 bits, but the descriptor gives usages to the first 116 only
constexpr HidUsage FirstInternational = 0x87;

} // namespace

std::optional<std::vector<HidUsage>> keysDown(std::span<const std::uint8_t> report)
{
    if (report.size() != KeyReportSize || report[0] != KeyReportId)
        return std::nullopt;
    std::vector<HidUsage> down;
    down.reserve(MaxKeysDown);
    for (unsigned bit = 0; bit < 8; ++bit) {
        if ((report[ModifierByte] >> bit & 1U) != 0)
            down.push_back(static_cast<HidUsage>(FirstModifier + bit));
    }
    for (HidUsage usage = FirstUsageThatIsAKey; usage <= LastBitmapUsage; ++usage) {
        if ((report[BitmapFirstByte + usage / 8U] >> (usage % 8U) & 1U) != 0)
            down.push_back(usage);
    }
    for (unsigned bit = 0; bit < 8; ++bit) {
        if ((report[InternationalByte] >> bit & 1U) != 0)
            down.push_back(static_cast<HidUsage>(FirstInternational + bit));
    }
    std::ranges::sort(down);
    return down;
}

} // namespace galleon::keyboard
