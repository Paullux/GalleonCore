// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "galleon/keyboard/BragiFrame.h"

#include <array>
#include <cstdint>

// Pure builders of the live-lighting requests other than the frame (LiveFrame.h): session start, get, set
// the mode, unbind, ping, open the live file. Every byte is the one of the capture in docs/protocol/README.md
// "Live per-key lighting (Windows, 2026-10-25)" (Aaryush1, issue #56), and a test checks that LiveCommandGuard
// lets each of them through: these builders can only produce what the allowlist allows.
//
// Reports are 129 bytes: report ID 00, direction 00, request marker 01, the session byte, the command, then
// the payload, zero padded.
namespace galleon::keyboard {

using Request = std::array<std::uint8_t, WriteSize>;

inline constexpr std::uint8_t ModeSelfOperated = 1; // the onboard profile runs the keys
inline constexpr std::uint8_t ModeHostControlled = 2; // the host's frames run the keys

// `random` are the 4 random bytes the session start carries; the caller draws them (nothing random here).
[[nodiscard]] Request sessionStartRequest(const std::array<std::uint8_t, 4> &random);
[[nodiscard]] Request getPropertyIn(std::uint8_t session, std::uint8_t property);
[[nodiscard]] Request setModeRequest(std::uint8_t session, std::uint8_t mode);
[[nodiscard]] Request unbindRequest(std::uint8_t session);
[[nodiscard]] Request pingRequest(std::uint8_t session);
[[nodiscard]] Request openLiveFileRequest(std::uint8_t session);

} // namespace galleon::keyboard
