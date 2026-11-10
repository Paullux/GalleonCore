// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

// Pure frame builders for the keyboard's vendor command interface ("Bragi"), independent of
// Protocol.h/StreamDeck.h (the Stream Deck module's own protocol, a different USB part: see
// AGENTS.md "Maintaining architecture over time").
//
// Format, from docs/protocol/README.md ("Keyboard brightness: property 0xEA", 2026-11-08) and the
// Web Hub capture it is built from, firmware 2.3.88: unnumbered 128-byte reports (hidapi wants a
// leading report-ID byte 00, so WriteSize is 129).
//   get request    00 01 00 02 <property>
//   set request    00 01 00 01 <property> 00 <value low byte> <value high byte>
//   get answer     00 00 00 02 <status> <value, 4 bytes little-endian>
//   set answer     00 00 00 01 <status>                          (no value: an echo)
namespace galleon::keyboard {

inline constexpr std::size_t PayloadSize = 128;
inline constexpr std::size_t WriteSize = PayloadSize + 1; // report ID byte 00, then the payload
inline constexpr std::uint8_t CommandSet = 0x01;
inline constexpr std::uint8_t CommandGet = 0x02;

// The 129 bytes to write for "read this property".
[[nodiscard]] std::array<std::uint8_t, WriteSize> getPropertyRequest(std::uint8_t property);

// The 129 bytes to write for "set this property to this 16-bit value" (little-endian on the
// wire). Never validates the value against what the property accepts: see Brightness.h for the
// brightness property's own range.
[[nodiscard]] std::array<std::uint8_t, WriteSize> setPropertyRequest(std::uint8_t property, std::uint16_t value);

} // namespace galleon::keyboard
