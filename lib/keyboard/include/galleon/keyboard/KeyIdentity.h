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

// Two keys of the Galleon 100 SD have no HID usage (the Fn key sends nothing the system sees, the Copilot key sends a combination), so
// they get identities of this project, in a range that no HID usage uses. They have no evdev code and no scan code: they are never
// read from a key press, only chosen by hand in the calibration.
inline constexpr HidUsage FnKey = 0xFF01;
inline constexpr HidUsage CopilotKey = 0xFF02;

// Linux: the evdev key code (KEY_*), as read from /dev/input or libinput. Beware that X11 and Wayland keycodes given by
// Qt's nativeScanCode are the evdev code plus 8: the caller subtracts 8 before calling.
[[nodiscard]] std::optional<HidUsage> hidUsageFromEvdev(unsigned code);

// Windows: the scan code of a key (bits 16 to 23 of lParam of WM_KEYDOWN) and whether the key is "extended" (bit 24).
[[nodiscard]] std::optional<HidUsage> hidUsageFromWindowsScanCode(unsigned scanCode, bool extended);

// What Qt's QKeyEvent::nativeScanCode holds, on each system, so a window can name the physical key without looking at the
// characters. On Linux (X11 and Wayland) it is the XKB keycode, the evdev code plus 8. On Windows it is the scan code of the key,
// with the "extended" (E0) flag read from bit 8 or from an 0xE0 high byte: both are accepted because the exact form is to be
// confirmed on a Windows machine; an unknown or ambiguous value gives no identity rather than a wrong one.
enum class QtScanCodeSystem : std::uint8_t { Linux, Windows };
[[nodiscard]] std::optional<HidUsage> hidUsageFromQtNativeScanCode(unsigned code, QtScanCodeSystem system);

// The reverse, for the tables' own tests and for showing a key's name in the calibration tool.
[[nodiscard]] std::optional<unsigned> evdevFromHidUsage(HidUsage usage);

// True for the keys whose engraving depends on the layout of the keyboard: letters, digits and punctuation (0x04 to 0x27, 0x2D to 0x38) and
// the ISO key next to the left Shift (0x64). The names of keyName() are those of a QWERTY keyboard, so for these keys they are not what is
// written on an AZERTY one: whoever shows such a name says so.
[[nodiscard]] bool isLayoutDependent(HidUsage usage);

// A short English name of the key ("A", "F1", "Left Control"), for a person to read; nullopt for a usage not in the tables.
[[nodiscard]] std::optional<const char *> keyName(HidUsage usage);

} // namespace galleon::keyboard
