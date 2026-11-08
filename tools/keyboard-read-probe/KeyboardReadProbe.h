// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <galleon/Transport.h>

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

// READ-ONLY reads of the keyboard's vendor interface through a galleon::Transport.
//
// Why this exists: before a keyboard library is written, prove that HidTransport (hidapi) can
// carry the request/answer exchange of tools/linux/bragi-probe.c and tools/windows/bragi-probe.ps1
// on both systems. It sends command 0x02 (read a property) for the properties listed below and
// nothing else: no mode change, no colour, no property write, no file command.
//
// Format, from docs/protocol/README.md ("Vendor interface seen from the Corsair Web Hub") and
// the two probes above, firmware 2.3.88: unnumbered 128-byte reports.
//   request  00 01 00 02 <property>              (hidapi wants a leading report-ID byte 00)
//   answer   00 00 00 02 <status> <value u32 little-endian>
// Not a driver: it waits for the answer by polling, which a daemon must not do on its loop.
namespace keyboardprobe {

inline constexpr std::size_t PayloadSize = 128;
inline constexpr std::size_t WriteSize = PayloadSize + 1; // report ID byte 00, then the payload
inline constexpr std::uint8_t CommandGet = 0x02;
inline constexpr std::uint8_t PropertyFirmware = 0x13;   // bytes 02 03 58 = firmware 2.3.88
inline constexpr std::uint8_t PropertyBrightness = 0xea; // 0 to 1000, steps of 200 in the Web Hub

// The only properties this tool will ask for.
inline constexpr std::array<std::uint8_t, 2> ReadableProperties{PropertyFirmware, PropertyBrightness};

struct PropertyReply
{
    std::uint8_t status = 0;
    std::uint32_t value = 0;
};

enum class ReadError { None, NotAllowed, WriteFailed, Disconnected, Timeout };

struct ReadResult
{
    ReadError error = ReadError::None;
    PropertyReply reply;
    std::vector<std::uint8_t> raw; // the accepted answer as the transport returned it
    int staleDiscarded = 0;        // reports already waiting before the request
    int unexpected = 0;            // reports received while waiting that were not our answer
};

[[nodiscard]] bool isReadableProperty(std::uint8_t property);

// The 129 bytes to write for "read this property".
[[nodiscard]] std::array<std::uint8_t, WriteSize> getPropertyRequest(std::uint8_t property);

// Accepts 128 bytes (unnumbered report as hidraw and hidapi return it) or 129 bytes with a leading
// 00; anything else, another command, or a truncated answer gives nothing.
[[nodiscard]] std::optional<PropertyReply> parsePropertyReply(std::span<const std::uint8_t> bytes);

[[nodiscard]] ReadResult readProperty(galleon::Transport &transport, std::uint8_t property,
                                      std::chrono::milliseconds timeout);

[[nodiscard]] const char *describe(ReadError error);

} // namespace keyboardprobe
