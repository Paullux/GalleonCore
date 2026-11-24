// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "galleon/keyboard/KeyIdentity.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

// The keys that are down in an input report of the keyboard (USB 1b1c:2b0c), as USB HID usages. PURE: it takes the bytes of one report and gives the
// usages; it opens nothing, keeps nothing and logs nothing. Whoever reads the reports (a later, separate component of galleond: off by default,
// explicit opt-in, see AGENTS.md "Security and privacy by design") uses what this returns at once and drops it.
//
// The layout is the one the keyboard's own report descriptor declares (docs/protocol/README.md, "Keyboard input reports", read on 2026-11-24 by
// tools/linux/hid-descriptors.c without any key press), and it is the report the keyboard actually sent in the owner's measurement of the same day
// (tools/linux/hid-input-shape.c: 87 reports in 15 s of typing, all of ID 1 and 18 bytes, none of ID 8):
//   byte 0      the report ID, 1
//   byte 1      the eight modifiers: bit 0 = usage 0xE0 (left Control) to bit 7 = 0xE7 (right GUI)
//   bytes 2-16  a bitmap of 120 bits: bit n (byte 2 + n / 8, bit n mod 8) stands for usage n, n = 0 to 0x73; the last four bits have no usage
//   byte 17     eight bits for usages 0x87 to 0x8E (the international keys)
// The keyboard also declares a report of ID 8 (modifiers and an array of 13 usages) and one of ID 2 (media keys). The first was never seen, the
// second is not a key of the keyboard, and so both are refused here as any report that is not exactly the one above. If ID 8 turns up on some
// firmware, it will be added with a test built from what was seen.
namespace galleon::keyboard {

inline constexpr std::uint8_t KeyReportId = 1;
inline constexpr std::size_t KeyReportSize = 18;
inline constexpr std::uint8_t MediaKeyReportId = 2;
inline constexpr std::size_t MediaKeyReportSize = 3;
// At most this many keys can be down in one report: 8 modifiers, usages 4 to 0x73 and the 8 international keys.
inline constexpr std::size_t MaxKeysDown = 8 + (0x73 - 4 + 1) + 8;

// The usages down in `report`, in increasing order, each once. The usages 0 to 3 (no event and the rollover and POST-fail codes, which are not keys)
// and the four bits without a usage are never reported. An empty list means no key is down. std::nullopt for anything that is not exactly 18 bytes
// with the ID 1: such a report is dropped by the caller, and what it held is not kept. Pure.
[[nodiscard]] std::optional<std::vector<HidUsage>> keysDown(std::span<const std::uint8_t> report);

// True for the report of the media keys (ID 2, 3 bytes: volume, play...). It is a report of the keyboard that is not a key of the lighting, so a reader
// drops it without a word, where any other report that is not the one above is dropped with one warning that gives its ID and length (never a byte of it). Pure.
[[nodiscard]] bool isMediaKeyReport(std::span<const std::uint8_t> report);

} // namespace galleon::keyboard
