// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "galleon/keyboard/BragiFrame.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

// Pure builders for the live (non-persistent) per-key lighting frame of the keyboard, independent of
// Protocol.h/StreamDeck.h. Nothing here sends anything: see LiveCommandGuard.h for what may be sent.
//
// Format, from docs/protocol/README.md "Live per-key lighting (Windows, 2026-10-25)" (measured by
// Aaryush1 on one keyboard, firmware 2.3.88, issue #56): the frame is `12 00` followed by 146 RGB
// triplets, one per LED index (= USB HID usage), 440 bytes in all. It is written into buffer 0 as one
// `06` report (buffer, length u32 LE, first data) and `07` continuations (buffer, data), each zero
// padded to 128 bytes. The worked example in that document is the test vector of this layer.
namespace galleon::keyboard {

inline constexpr std::size_t LiveLedCount = 146; // indices 0x00 to 0x91
inline constexpr std::size_t LiveFrameSize = 2 + 3 * LiveLedCount;
inline constexpr std::uint8_t CommandWriteBegin = 0x06;
inline constexpr std::uint8_t CommandWriteMore = 0x07;
inline constexpr std::size_t LiveReportCount = 4;
// Data bytes carried by the first report (after the 4 header bytes, buffer byte and 4 length bytes) and by
// each continuation (after the 4 header bytes and the buffer byte).
inline constexpr std::size_t WriteBeginData = PayloadSize - 4 - 1 - 4;
inline constexpr std::size_t WriteMoreData = PayloadSize - 4 - 1;

struct LedColor
{
    std::uint16_t led = 0;
    std::uint8_t red = 0;
    std::uint8_t green = 0;
    std::uint8_t blue = 0;
};

using LiveFrame = std::array<std::uint8_t, LiveFrameSize>;
using LiveReports = std::array<std::array<std::uint8_t, WriteSize>, LiveReportCount>;

// All LEDs black except the ones listed. A later entry for the same LED replaces an earlier one. A LED
// index of 146 or more refuses the whole frame (nothing is dropped silently), as does an empty list.
[[nodiscard]] std::optional<LiveFrame> buildLiveFrame(std::span<const LedColor> leds);

// The four 129-byte writes (report ID 00 first) that carry the frame under this session ID. Session 0 means
// "no session open" and is refused: a frame is only ever written inside a session.
[[nodiscard]] std::optional<LiveReports> liveFrameReports(const LiveFrame &frame, std::uint8_t session);

} // namespace galleon::keyboard
