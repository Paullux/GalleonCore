// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "galleon/keyboard/KeyIdentity.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <optional>

namespace galleon::keyboard {

namespace {

struct KeyRow
{
    HidUsage usage;
    unsigned evdev;
    unsigned winScan;  // 0 = none (the key sends no single scan code)
    bool winExtended;  // the scan code carries the E0 prefix
    const char *name;
};

// One row per key, so both conversions come from the same source. The ISO key next to Enter (HID 0x32, "#~") is not
// listed: on Linux and on Windows it sends the same code as the ANSI backslash (HID 0x31), so the code cannot tell
// the two apart; the calibration records the pairing the person sees.
constexpr std::array<KeyRow, 90> Keys{{
    {0x04, 30, 0x1E, false, "A"},
    {0x05, 48, 0x30, false, "B"},
    {0x06, 46, 0x2E, false, "C"},
    {0x07, 32, 0x20, false, "D"},
    {0x08, 18, 0x12, false, "E"},
    {0x09, 33, 0x21, false, "F"},
    {0x0A, 34, 0x22, false, "G"},
    {0x0B, 35, 0x23, false, "H"},
    {0x0C, 23, 0x17, false, "I"},
    {0x0D, 36, 0x24, false, "J"},
    {0x0E, 37, 0x25, false, "K"},
    {0x0F, 38, 0x26, false, "L"},
    {0x10, 50, 0x32, false, "M"},
    {0x11, 49, 0x31, false, "N"},
    {0x12, 24, 0x18, false, "O"},
    {0x13, 25, 0x19, false, "P"},
    {0x14, 16, 0x10, false, "Q"},
    {0x15, 19, 0x13, false, "R"},
    {0x16, 31, 0x1F, false, "S"},
    {0x17, 20, 0x14, false, "T"},
    {0x18, 22, 0x16, false, "U"},
    {0x19, 47, 0x2F, false, "V"},
    {0x1A, 17, 0x11, false, "W"},
    {0x1B, 45, 0x2D, false, "X"},
    {0x1C, 21, 0x15, false, "Y"},
    {0x1D, 44, 0x2C, false, "Z"},
    {0x1E, 2, 0x02, false, "1"},
    {0x1F, 3, 0x03, false, "2"},
    {0x20, 4, 0x04, false, "3"},
    {0x21, 5, 0x05, false, "4"},
    {0x22, 6, 0x06, false, "5"},
    {0x23, 7, 0x07, false, "6"},
    {0x24, 8, 0x08, false, "7"},
    {0x25, 9, 0x09, false, "8"},
    {0x26, 10, 0x0A, false, "9"},
    {0x27, 11, 0x0B, false, "0"},
    {0x28, 28, 0x1C, false, "Enter"},
    {0x29, 1, 0x01, false, "Escape"},
    {0x2A, 14, 0x0E, false, "Backspace"},
    {0x2B, 15, 0x0F, false, "Tab"},
    {0x2C, 57, 0x39, false, "Space"},
    {0x2D, 12, 0x0C, false, "Minus"},
    {0x2E, 13, 0x0D, false, "Equal"},
    {0x2F, 26, 0x1A, false, "Left Bracket"},
    {0x30, 27, 0x1B, false, "Right Bracket"},
    {0x31, 43, 0x2B, false, "Backslash"},
    {0x33, 39, 0x27, false, "Semicolon"},
    {0x34, 40, 0x28, false, "Apostrophe"},
    {0x35, 41, 0x29, false, "Grave"},
    {0x36, 51, 0x33, false, "Comma"},
    {0x37, 52, 0x34, false, "Period"},
    {0x38, 53, 0x35, false, "Slash"},
    {0x39, 58, 0x3A, false, "Caps Lock"},
    {0x3A, 59, 0x3B, false, "F1"},
    {0x3B, 60, 0x3C, false, "F2"},
    {0x3C, 61, 0x3D, false, "F3"},
    {0x3D, 62, 0x3E, false, "F4"},
    {0x3E, 63, 0x3F, false, "F5"},
    {0x3F, 64, 0x40, false, "F6"},
    {0x40, 65, 0x41, false, "F7"},
    {0x41, 66, 0x42, false, "F8"},
    {0x42, 67, 0x43, false, "F9"},
    {0x43, 68, 0x44, false, "F10"},
    {0x44, 87, 0x57, false, "F11"},
    {0x45, 88, 0x58, false, "F12"},
    {0x46, 99, 0x37, true, "Print Screen"},
    {0x47, 70, 0x46, false, "Scroll Lock"},
    {0x48, 119, 0, false, "Pause"},
    {0x49, 110, 0x52, true, "Insert"},
    {0x4A, 102, 0x47, true, "Home"},
    {0x4B, 104, 0x49, true, "Page Up"},
    {0x4C, 111, 0x53, true, "Delete"},
    {0x4D, 107, 0x4F, true, "End"},
    {0x4E, 109, 0x51, true, "Page Down"},
    {0x4F, 106, 0x4D, true, "Right Arrow"},
    {0x50, 105, 0x4B, true, "Left Arrow"},
    {0x51, 108, 0x50, true, "Down Arrow"},
    {0x52, 103, 0x48, true, "Up Arrow"},
    {0x64, 86, 0x56, false, "ISO Less-than"},
    {0x65, 127, 0x5D, true, "Menu"},
    {0xE0, 29, 0x1D, false, "Left Control"},
    {0xE1, 42, 0x2A, false, "Left Shift"},
    {0xE2, 56, 0x38, false, "Left Alt"},
    {0xE3, 125, 0x5B, true, "Left Windows"},
    {0xE4, 97, 0x1D, true, "Right Control"},
    {0xE5, 54, 0x36, false, "Right Shift"},
    {0xE6, 100, 0x38, true, "Right Alt"},
    {0xE7, 126, 0x5C, true, "Right Windows"},
    // Keys with no HID usage on this keyboard (they send no code of their own, or a combination): identities of this project,
    // in a range that no HID usage uses, with no evdev code and no scan code. Chosen by hand in the calibration.
    {FnKey, 0, 0, false, "Fn"},
    {CopilotKey, 0, 0, false, "Copilot"},
}};

template <typename Predicate>
[[nodiscard]] const KeyRow *findKey(Predicate matches)
{
    const auto it = std::ranges::find_if(Keys, matches);
    return it == Keys.end() ? nullptr : &*it;
}

} // namespace

std::optional<HidUsage> hidUsageFromEvdev(unsigned code)
{
    const KeyRow *row = code == 0 ? nullptr : findKey([code](const KeyRow &key) { return key.evdev == code; });
    return row == nullptr ? std::nullopt : std::optional<HidUsage>(row->usage);
}

std::optional<HidUsage> hidUsageFromWindowsScanCode(unsigned scanCode, bool extended)
{
    if (scanCode == 0)
        return std::nullopt; // 0 is "no scan code" in the table, never a key
    const KeyRow *row = findKey([scanCode, extended](const KeyRow &key) {
        return key.winScan == scanCode && key.winExtended == extended;
    });
    return row == nullptr ? std::nullopt : std::optional<HidUsage>(row->usage);
}

std::optional<HidUsage> hidUsageFromQtNativeScanCode(unsigned code, QtScanCodeSystem system)
{
    switch (system) {
    case QtScanCodeSystem::Linux:
        return code > 8 ? hidUsageFromEvdev(code - 8) : std::nullopt;
    case QtScanCodeSystem::Windows: {
        const unsigned high = code & 0xFF00U;
        // Three forms are known: the bare scan code, with bit 8 set, or with an 0xE0 high byte (both of the last two mean "extended").
        const bool knownForm = code <= 0xFFFFU && (high == 0 || high == 0x100U || high == 0xE000U);
        if (!knownForm)
            return std::nullopt;
        return hidUsageFromWindowsScanCode(code & 0xFFU, high != 0);
    }
    }
    return std::nullopt;
}

std::optional<unsigned> evdevFromHidUsage(HidUsage usage)
{
    const KeyRow *row = findKey([usage](const KeyRow &key) { return key.usage == usage; });
    return row == nullptr || row->evdev == 0 ? std::nullopt : std::optional<unsigned>(row->evdev); // 0 = no evdev code
}

bool isLayoutDependent(HidUsage usage)
{
    return (usage >= 0x04 && usage <= 0x27) || (usage >= 0x2D && usage <= 0x38) || usage == 0x64;
}

std::optional<const char *> keyName(HidUsage usage)
{
    const KeyRow *row = findKey([usage](const KeyRow &key) { return key.usage == usage; });
    return row == nullptr ? std::nullopt : std::optional<const char *>(row->name);
}

std::optional<HidUsage> hidUsageFromKeyName(std::string_view name)
{
    const auto same = [name](const char *known) {
        const std::string_view other(known);
        return other.size() == name.size()
               && std::equal(name.begin(), name.end(), other.begin(), [](char a, char b) { return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b)); });
    };
    const KeyRow *row = findKey([&same](const KeyRow &key) { return same(key.name); });
    return row == nullptr ? std::nullopt : std::optional<HidUsage>(row->usage);
}

} // namespace galleon::keyboard
