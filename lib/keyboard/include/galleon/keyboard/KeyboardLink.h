// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <galleon/Transport.h>

#include <chrono>
#include <cstdint>

// Reads and writes keyboard property 0xEA (brightness) over a galleon::Transport, with the same
// stale-report/timeout discipline as tools/keyboard-read-probe/KeyboardReadProbe.cpp, but as
// independent code: this file must never include Protocol.h or StreamDeck.h (see AGENTS.md
// "Maintaining architecture over time" and docs/ROADMAP.md section 3, "Règles de séparation").
//
// Out of scope here, deferred (see docs/ROADMAP.md "Luminosité du clavier"): remembering the
// value to restore it on exit or after a crash, and yielding to the Web Hub or another host.
// writeBrightness only writes and reads the value back to confirm; it does not decide whether
// writing is safe to attempt right now.
namespace galleon::keyboard {

enum class LinkError { None, InvalidValue, WriteFailed, Disconnected, Timeout, Unconfirmed,
                       StaleReports, // the queue never emptied within the purge limit: an answer could belong to someone else
                       InvalidReply, // the answer carries a brightness outside 0..1000
                       SessionExpired // the keyboard says the session ended (byte 1 of its answer is 15): start another
};

struct ReadResult
{
    LinkError error = LinkError::None;
    std::uint8_t status = 0;
    std::uint16_t value = 0;
};

struct WriteResult
{
    LinkError error = LinkError::None;
    std::uint8_t status = 0;     // status of the set answer (echo)
    std::uint16_t confirmed = 0; // value read back after the write
};

[[nodiscard]] ReadResult readBrightness(Transport &transport, std::chrono::milliseconds timeout);

// Rejects a value outside 0..1000 before sending anything (LinkError::InvalidValue). Otherwise
// sends the set request, waits for its echo, then reads the property back to confirm; the echo's
// own status is kept in WriteResult::status but does not by itself decide success. Unconfirmed
// means the read-back value did not match what was requested.
[[nodiscard]] WriteResult writeBrightness(Transport &transport, std::uint16_t value, std::chrono::milliseconds timeout);

[[nodiscard]] const char *describe(LinkError error);

} // namespace galleon::keyboard
