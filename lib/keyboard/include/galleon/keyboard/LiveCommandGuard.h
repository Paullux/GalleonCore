// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "galleon/keyboard/BragiFrame.h"

#include <cstdint>
#include <span>
#include <cstddef>

// The allowlist of everything the live-lighting layer may ever send to the keyboard, as pure code: a report
// that does not match is refused BEFORE it reaches a transport, so a mistake in a higher layer cannot reach
// the device (docs/WINDOWS_LIVE_LIGHTING_CSHARP.md, "Transport allowlist"; rule 5 of its lessons).
//
// What is allowed, with the bytes of the capture in docs/protocol/README.md "Live per-key lighting
// (Windows, 2026-10-25)" (Aaryush1, issue #56, firmware 2.3.88). Reports are 129 bytes: report ID 00, then
// `00 01 <session> <command>` and the payload, zero padded.
//   0x1b session start   session byte 0, payload `01 <4 bytes> 00...`
//   0x02 get property    any property, nothing after it
//   0x01 set property    property 3 only, to 1 (self-operated) or 2 (host-controlled): never 3 or 5 (bootloaders)
//                        and never 4 (sleep), never any other property, so never the brightness (0xEA, stored)
//   0x12 ping            no payload
//   0x0d open file       buffer 0, file 0x0022 only (the volatile live frame), nothing else
//   0x05 unbind          payload `01 00`, exactly as in the capture (its meaning is not established beyond it)
//   0x06 / 0x07 write    buffer 0 only, only while file 0x0022 is open, exactly one 440-byte frame in the
//                        shape the capture shows (a `06` carrying length 440, then `07` continuations, none
//                        before the `06` or after the last byte)
// Everything else is refused: file creation or deletion, factory reset (0x0f), calibration, pairing, session
// stop, any property write that is not the mode, and any report whose unused bytes are not zero.
//
// Sequence is part of the rule: the guard keeps the little state needed for it (file open or not, bytes of
// the frame still expected). It is told what the keyboard confirmed with `confirmed`, never what was merely
// asked, so a lost answer cannot make it believe in a state the keyboard is not in.
namespace galleon::keyboard {

enum class Refusal : std::uint8_t {
    None,
    BadLength,         // not 129 bytes
    BadHeader,         // report ID, direction byte or request marker wrong
    UnknownCommand,    // not on the list
    BadPayload,        // a byte that must be zero is not, or a fixed byte differs
    PropertyNotAllowed,
    ModeNotAllowed,    // property 3 to anything but 1 or 2
    FileNotAllowed,
    BufferNotAllowed,  // anything but buffer 0
    NotOpen,           // a write while file 0x0022 is not open
    BadFrameLength,    // a `06` that does not announce exactly 440 bytes
    OutOfSequence      // a `07` without its `06`, a `06` inside a frame, a `07` past the last byte
};

[[nodiscard]] const char *describe(Refusal refusal);

inline constexpr std::uint8_t CommandSessionStart = 0x1b;
inline constexpr std::uint8_t CommandPing = 0x12;
inline constexpr std::uint8_t CommandOpenFile = 0x0d;
inline constexpr std::uint8_t CommandUnbind = 0x05;
inline constexpr std::uint8_t PropertyMode = 0x03;
inline constexpr std::uint16_t LiveFileId = 0x0022;

class LiveCommandGuard
{
public:
    // None when `report` may be sent in the current state; otherwise why not. Does not change the state.
    [[nodiscard]] Refusal check(std::span<const std::uint8_t> report) const;

    // Call with a report that check() allowed, once the keyboard answered it with status 0. A report that
    // was refused, or a state change that did not happen, must not be passed here.
    void confirmed(std::span<const std::uint8_t> report);

    // Forget the open file and any half-written frame (after a failure or a release).
    void reset();

    [[nodiscard]] bool fileOpen() const { return m_open; }
    [[nodiscard]] std::size_t bytesExpected() const { return m_expected; }

private:
    bool m_open = false;
    std::size_t m_expected = 0; // bytes of the frame still to come after the last confirmed write
};

} // namespace galleon::keyboard
