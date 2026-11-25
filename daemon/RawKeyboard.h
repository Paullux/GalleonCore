// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <galleon/keyboard/KeyIdentity.h>

#include <QString>

#include <optional>

namespace galleond {

// What Windows tells of a key in a Raw Input message (RAWKEYBOARD of winuser.h), as far as the key glow needs: the scan code and the flags. PURE: no Windows header
// here, so that what is decided from it is tested on every system. The values of the flags are those of winuser.h (RI_KEY_BREAK, RI_KEY_E0, RI_KEY_E1; the Windows
// source checks them with static_assert against the real macros).
inline constexpr unsigned RawKeyBreak = 0x01; // the key goes UP (RI_KEY_BREAK); without it, the key goes down (RI_KEY_MAKE is 0)
inline constexpr unsigned RawKeyE0 = 0x02;    // the scan code is the extended one (RI_KEY_E0)
inline constexpr unsigned RawKeyE1 = 0x04;    // the sequence of the Pause key (RI_KEY_E1)

struct RawKeyboardEvent
{
    unsigned makeCode = 0; // the scan code
    unsigned flags = 0;
};

// The usage of the key that WENT DOWN, from the table of KeyIdentity.h; nothing for a key that went up, for the sequence of the Pause key (several codes: not a key of
// the lighting) and for a scan code that is not in the table (0, the fake codes that Windows uses for overruns, keys of other layouts). Pure.
[[nodiscard]] std::optional<galleon::keyboard::HidUsage> usageOfRawKeyDown(const RawKeyboardEvent &event);

// True for the name of a Raw Input device that is the Galleon 100 SD keyboard (USB 1b1c:2b0c): `\\?\HID#VID_1B1C&PID_2B0C&MI_00#...`. The comparison ignores the
// case, and the product number must end there (a PID_2B0C1 is another product). The Stream Deck module (PID_2B18) and every other keyboard are refused. Pure.
[[nodiscard]] bool isGalleonKeyboardDeviceName(const QString &name);

} // namespace galleond
