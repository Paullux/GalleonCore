// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <chrono>
#include <cstdint>

// Pure pacing and failure policy for the live lighting stream. Time is an argument, never read here, so
// every rule is tested without waiting. Values and reasons from docs/WINDOWS_LIVE_LIGHTING_CSHARP.md
// (sections "Freeze policy" and "Rate limit", Aaryush1, issue #56).
namespace galleon::keyboard {

using LiveClock = std::chrono::steady_clock;

// At most `ratePerSecond` frames on average with a burst of `burst`, as a token bucket. A frame that comes
// too early is refused, not queued: the caller drops it. The cap follows the limit seen on other Bragi
// keyboards (faster streaming stalled keystrokes there); the real limit of this keyboard is not known.
class RateLimiter
{
public:
    static constexpr double DefaultRate = 20.0;
    static constexpr double DefaultBurst = 2.0;

    explicit RateLimiter(LiveClock::time_point start, double ratePerSecond = DefaultRate, double burst = DefaultBurst);

    // True and one token spent when a frame may go out at `now`. A clock that went backwards adds nothing.
    [[nodiscard]] bool tryTake(LiveClock::time_point now);

private:
    double m_rate;
    double m_burst;
    double m_tokens;
    LiveClock::time_point m_last;
};

// Two consecutive failures (a timeout or a non-zero status) trip it; any success in between resets the count.
// Once tripped it stays tripped until reset() (the keyboard was unplugged and came back): earlier frame
// layouts froze typing until unplug, so the stream does not try again by itself.
class FreezePolicy
{
public:
    static constexpr int Threshold = 2;

    void success();
    void failure();
    void reset();
    [[nodiscard]] bool tripped() const { return m_tripped; }

private:
    int m_failures = 0;
    bool m_tripped = false;
};

} // namespace galleon::keyboard
