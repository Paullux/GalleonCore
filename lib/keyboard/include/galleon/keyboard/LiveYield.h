// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "galleon/keyboard/LivePolicy.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

// Giving way to another controller (the Web Hub, any other host), as pure decisions: no I/O, and time is an
// argument. The rules and their numbers come from docs/WINDOWS_LIVE_LIGHTING_CSHARP.md, sections "Yield,
// never fight", "Release is the host's job" and "Lessons that apply to any implementation" (Aaryush1,
// issue #56, measured with the Web Hub opened during a run, firmware 2.3.88). They were measured on one
// keyboard under Windows; nothing here has been tried on Linux.
//
// Why: when the Web Hub connects it sets the mode to 1, yet the keyboard keeps answering our frames with
// status 0, so there is NO error to notice. Two hosts writing make the keys flicker. The signs of another
// host are (1) the mode is not the 2 we set and (2) an answer nobody here asked for.
namespace galleon::keyboard {

// ---- classifying the answers -----------------------------------------------------------------------

enum class ReplyKind : std::uint8_t {
    Ours,   // the answer to the command being waited for
    Stale,  // a late answer to one of our own commands that already timed out: not another host
    Foreign // an answer nobody here asked for: another host is polling the keyboard
};

// Every answer is classified against the command being waited for. Answers carry no host identity, only the
// command they echo (byte 3 of the report), so an answer to ANOTHER host's command that happens to echo the
// same command as ours cannot be told apart: the mode check below is the second line of defence.
class ReplyTracker
{
public:
    static constexpr std::size_t MaxStale = 8; // bounded: a long run never grows this list

    void expect(std::uint8_t command);      // a command is sent and its answer awaited
    void timedOut();                        // gave up on it: its answer may still arrive, as Stale
    [[nodiscard]] ReplyKind classify(std::uint8_t command); // an answer arrived

private:
    std::optional<std::uint8_t> m_waiting;
    std::vector<std::uint8_t> m_stale;
};

// ---- yielding ----------------------------------------------------------------------------------------

enum class LiveState : std::uint8_t {
    Listening, // just connected: nothing is sent for ListenFor, to hear whether another host is already talking
    Holding,   // the keys are ours; the mode is re-read about once a second
    Yielded    // another host is there: NOTHING is sent, not even a ping, and nothing is released
};

enum class YieldAction : std::uint8_t {
    None,
    StartHolding,    // the listening time passed in silence: the keys may be taken (mode 2)
    ReadMode,        // read property 3 now (the one allowed send in Holding's poll and in Yielded's probe)
    ReleaseAndResume // the other host left: release (unbind, mode 1, read back) and take the keys again
};

class YieldPolicy
{
public:
    static constexpr auto ListenFor = std::chrono::seconds(3);
    static constexpr auto ModePollEvery = std::chrono::seconds(1);
    static constexpr auto QuietFor = std::chrono::seconds(60);

    explicit YieldPolicy(LiveClock::time_point connected);

    [[nodiscard]] LiveState state() const { return m_state; }
    // Frames may be written only while Holding.
    [[nodiscard]] bool mayWrite() const { return m_state == LiveState::Holding; }
    // A release is never sent while Yielded (an unbind could break the other host's file transfer); the only
    // release is the one ReleaseAndResume asks for.
    [[nodiscard]] bool mayRelease() const { return m_state != LiveState::Yielded; }

    // What to do now; call it often (a timer of the order of 100 ms is enough).
    [[nodiscard]] YieldAction tick(LiveClock::time_point now);

    // An answer nobody asked for was seen (ReplyTracker said Foreign). Starts yielded when still listening,
    // and restarts the quiet period when already yielded.
    void foreignTraffic(LiveClock::time_point now);

    // The mode (property 3) was read, as a poll while Holding or as the probe after the quiet period.
    [[nodiscard]] YieldAction modeRead(std::uint8_t mode, LiveClock::time_point now);

private:
    LiveState m_state = LiveState::Listening;
    LiveClock::time_point m_listenFrom;
    LiveClock::time_point m_lastPoll;
    LiveClock::time_point m_lastForeign;
    bool m_probing = false;
};

// ---- start and watchdog decisions --------------------------------------------------------------------

enum class StartDecision : std::uint8_t {
    Proceed,       // the keyboard is in its own mode (1): nothing to undo
    ReleaseFirst,  // a leftover of a crash, sleep or kill: release, then proceed
    StartYielded   // another host is already talking: send nothing, release nothing
};

// At every start the mode is read first and the keyboard released unless it is a CONFIRMED 1 (a failed read
// is not a confirmed 1). Unless another host was heard while listening: then nothing is sent at all.
[[nodiscard]] StartDecision startDecision(bool heardAnotherHost, std::optional<std::uint8_t> mode);

// The separate watchdog process (started with the writer's PID) acts when the writer is gone: it releases only
// if the keyboard is still in host mode (2). A mode of 1 means the writer had released already; anything else,
// or an unreadable mode, is not its business and nothing is sent.
[[nodiscard]] bool watchdogShouldRelease(std::optional<std::uint8_t> mode);

} // namespace galleon::keyboard
