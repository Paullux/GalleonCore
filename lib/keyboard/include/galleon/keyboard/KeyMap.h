// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "galleon/keyboard/KeyIdentity.h"
#include "galleon/keyboard/LiveFrame.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

// The map between the LEDs of the live frame (indices 0 to 145, see LiveFrame.h) and the physical keys they light.
// A model only: no I/O and nothing is sent. It describes the hardware (which LED is which key), never how it was
// used: it holds no key press, no order and no time (AGENTS.md, "Security and privacy by design").
//
// The only starting point that is documented is a hypothesis, not a measurement: for the keys checked so far the LED
// index is the key's HID usage (docs/protocol/README.md, "LED indices"). assumedLedForKey() gives that hypothesis so a
// calibration can propose it and a person can confirm or correct it; a value in the map itself comes only from a
// confirmation.
namespace galleon::keyboard {

// What a LED is, as someone looking at the keyboard saw it.
enum class LedKind : std::uint8_t {
    Key,   // lights one physical key
    Strip, // lights a section of the light strip along the top edge
    Unlit  // lit nothing that could be seen
};

struct LedAssignment
{
    std::uint16_t led = 0;
    LedKind kind = LedKind::Unlit;
    HidUsage key = 0;            // only for Kind::Key
    std::uint8_t stripPosition = 0; // only for Kind::Strip: 1 = the leftmost section seen, counting to the right

    friend bool operator==(const LedAssignment &, const LedAssignment &) = default;
};

enum class AssignError : std::uint8_t {
    LedOutOfRange,    // not one of the 146 LEDs of the live frame
    UnknownKey,       // the key is not in the table of KeyIdentity.h
    KeyAlreadyUsed,   // another LED already lights that key
    BadStripPosition, // a strip section 0 or beyond the bound
    MissingDetail     // a kind that needs a key or a position and has none
};

// At most this many strip sections are accepted (the light strip has far fewer lamps than 146).
inline constexpr std::uint8_t MaxStripPosition = 64;

class KeyMap
{
public:
    // Sets what a LED is, replacing an earlier answer for that same LED. A refused assignment changes nothing.
    [[nodiscard]] std::optional<AssignError> assign(const LedAssignment &assignment);

    [[nodiscard]] std::optional<LedAssignment> assignmentFor(std::uint16_t led) const;
    [[nodiscard]] std::optional<std::uint16_t> ledForKey(HidUsage key) const;
    [[nodiscard]] std::vector<std::uint16_t> unassignedLeds() const;
    // Every answer recorded, by increasing LED index.
    [[nodiscard]] std::vector<LedAssignment> assignments() const;
    [[nodiscard]] std::size_t assignedCount() const;

private:
    std::array<std::optional<LedAssignment>, LiveLedCount> m_leds{};
};

// The hypothesis of the documentation (not a measurement): the LED of a key is its HID usage, when that usage is within
// the 146 LEDs. nullopt for a usage beyond them (the modifiers 0xE0 to 0xE7) or not in the key table.
[[nodiscard]] std::optional<std::uint16_t> assumedLedForKey(HidUsage key);

} // namespace galleon::keyboard
