// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <cstdint>
#include <optional>

// The identity of a PHYSICAL key, independent of the layout (AZERTY, QWERTY...) and of the characters the system
// produces: the USB HID usage of the key on the Keyboard/Keypad page (0x07). It is also what the keyboard's lighting
// engine uses to address most keys (docs/protocol/README.md, "LED indices": the LED index is the HID usage for the keys
// checked so far), so one number serves both the key and its LED.
//
// This file is pure: only tables and conversions. Nothing here observes the keyboard. Whoever reads key presses
// (a later, separate component) must follow the privacy rules of AGENTS.md ("Security and privacy by design"):
// the identity of a pressed key is used at once and never stored, logged or passed on.
//
// Sources of the tables in KeyIdentity.cpp:
//   - HID usages: USB HID Usage Tables, Keyboard/Keypad page (0x07).
//   - evdev codes: the Linux kernel's input-event-codes.h (checked against the header of the build machine).
//   - Windows scan codes: Microsoft's "Keyboard scan code specification" (set 1 make codes; the extended ones
//     start with E0 and are given here as `extended`).
// Not covered yet: the numeric keypad (the Galleon 100 SD has none: the Stream Deck module takes its place), the
// media keys and the Pause key under Windows (it sends a multi-byte sequence, not a scan code).
namespace galleon::keyboard {

// A USB HID usage of the Keyboard/Keypad page (0x07), e.g. 0x04 = A, 0x3A = F1, 0xE0 = left Control.
using HidUsage = std::uint16_t;

// Linux: the evdev key code (KEY_*), as read from /dev/input or libinput. Beware that X11 and Wayland keycodes given by
// Qt's nativeScanCode are the evdev code plus 8: the caller subtracts 8 before calling.
[[nodiscard]] std::optional<HidUsage> hidUsageFromEvdev(unsigned code);

// Windows: the scan code of a key (bits 16 to 23 of lParam of WM_KEYDOWN) and whether the key is "extended" (bit 24).
[[nodiscard]] std::optional<HidUsage> hidUsageFromWindowsScanCode(unsigned scanCode, bool extended);

// The reverse, for the tables' own tests and for showing a key's name in the calibration tool.
[[nodiscard]] std::optional<unsigned> evdevFromHidUsage(HidUsage usage);

// A short English name of the key ("A", "F1", "Left Control"), for a person to read; nullopt for a usage not in the tables.
[[nodiscard]] std::optional<const char *> keyName(HidUsage usage);

} // namespace galleon::keyboard
