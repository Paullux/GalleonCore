// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "galleon/keyboard/LiveFrame.h"
#include "galleon/keyboard/LivePolicy.h"
#include "galleon/keyboard/LiveRelease.h"
#include "galleon/keyboard/LiveYield.h"

#include <galleon/Transport.h>

#include <array>
#include <chrono>
#include <cstdint>
#include <memory>
#include <vector>

// The live per-key lighting of the keyboard over a galleon::Transport: every safety piece of this library put
// together (the allowlist, the pacing, the freeze rule, the yielding to another host and the release), driven by
// the caller's clock. It is the only thing that takes the keyboard into host mode and the only thing that gives it
// back. Rules and numbers: docs/WINDOWS_LIVE_LIGHTING_CSHARP.md (Aaryush1, issue #56), measured on one keyboard,
// firmware 2.3.88, under Windows; nothing here has been tried on real hardware.
//
// Nothing is sent by itself and nothing here waits: the caller calls tick() every 50 to 100 ms, show() when it has a
// frame, and release() when it stops. Everything blocks only for the length of one exchange (the timeout).
//
// What it promises, and what the tests check:
// - Nothing is sent for 3 s after begin(), to hear whether another host is already talking; if one is, nothing is ever
//   sent until it has been quiet for 60 s.
// - Every report that leaves passes LiveCommandGuard, and only the five kinds of report the allowlist knows.
// - The keys are taken (mode 2, the live file open) only after the mode was read: a mode that is not a confirmed 1 is
//   released first.
// - A mode that is not 2, or an answer nobody here asked for, makes it yield: it stops sending anything, and does not
//   release (an unbind could break the other host's file transfer). It resumes after 60 s of quiet, by releasing and
//   taking the keys again.
// - Frames are paced at 20 per second at most (the rest are dropped, not queued), and two failures in a row stop the
//   stream for good until the keyboard is plugged in again.
// - release() gives the keys back (unbind, mode 1, read back) and says whether it could.
// It never writes a property but the mode, never opens a file but the live one, never writes to the keyboard's own
// memory. Whoever runs this also runs the watchdog (tools/keyboard-watchdog) with its own process ID first.
namespace galleon::keyboard {

class LiveLink
{
public:
    enum class Status : std::uint8_t {
        Idle,         // begin() was not called
        Listening,    // the first 3 s: nothing is sent
        Holding,      // the keys are ours (or are being taken: see keysTaken())
        Yielded,      // another host is there: nothing is sent
        Frozen,       // two failures in a row: nothing more until resetAfterReplug()
        Disconnected, // the transport says the keyboard is gone
        Released      // release() was called
    };

    enum class ShowResult : std::uint8_t {
        Sent,
        Dropped,    // too soon after the last frame (20 per second at most)
        NotHolding, // listening, yielded, idle, released, or the keys are not taken yet
        Frozen,
        Failed      // the frame did not go through (counts toward the freeze)
    };

    struct Settings
    {
        std::chrono::milliseconds timeout{500}; // for one request and its answer
        std::array<std::uint8_t, 4> sessionRandom{}; // the 4 random bytes of the session start (the caller draws them)
    };

    LiveLink(Transport &transport, Settings settings);
    ~LiveLink();
    LiveLink(const LiveLink &) = delete;
    LiveLink &operator=(const LiveLink &) = delete;

    // Starts listening. Nothing is sent before 3 s have passed.
    void begin(LiveClock::time_point now);
    // What is due: the first start, the mode poll while holding, the probe while yielded. Cheap when nothing is.
    void tick(LiveClock::time_point now);
    // One frame; see ShowResult.
    [[nodiscard]] ShowResult show(const LiveFrame &frame, LiveClock::time_point now);
    // Gives the keys back. Does nothing, and says NotNeeded, while yielded (or before begin): another host has them.
    [[nodiscard]] ReleaseOutcome release(LiveClock::time_point now);
    // The keyboard was plugged in again: the freeze is lifted and everything starts over from Idle.
    void resetAfterReplug();

    [[nodiscard]] Status status() const;
    // True once mode 2 is set and the live file is open. Frames go only then.
    [[nodiscard]] bool keysTaken() const { return m_taken; }
    // Goes up each time the keys were (re)taken: a caller that keeps a scene sends it again when this changes.
    [[nodiscard]] std::uint32_t generation() const { return m_generation; }
    [[nodiscard]] LinkError lastError() const { return m_lastError; }

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
    bool m_taken = false;
    std::uint32_t m_generation = 0;
    LinkError m_lastError = LinkError::None;
};

[[nodiscard]] const char *describe(LiveLink::Status status);

} // namespace galleon::keyboard
