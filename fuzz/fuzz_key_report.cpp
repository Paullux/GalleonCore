// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <galleon/keyboard/KeyReport.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>

using namespace galleon::keyboard;

// Run: ./fuzz_key_report -max_total_time=60
// The input report of the keyboard comes from the device, which is not trusted: whatever the bytes, only a report of exactly 18 bytes with the ID 1
// is accepted, and what it gives is a bounded, increasing list of usages that are keys, each once.
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size)
{
    const auto usages = keysDown(std::span<const std::uint8_t>(data, size));
    const bool shapeIsRight = size == KeyReportSize && data[0] == KeyReportId;
    if (usages.has_value() != shapeIsRight)
        __builtin_trap();
    if (!usages)
        return 0;
    if (usages->size() > MaxKeysDown || !std::ranges::is_sorted(*usages) || std::ranges::adjacent_find(*usages) != usages->end())
        __builtin_trap();
    for (const HidUsage usage : *usages) {
        const bool modifier = usage >= 0xE0 && usage <= 0xE7;
        const bool bitmap = usage >= 4 && usage <= 0x73;
        const bool international = usage >= 0x87 && usage <= 0x8E;
        if (!modifier && !bitmap && !international)
            __builtin_trap();
    }
    return 0;
}
