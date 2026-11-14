// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "galleon/keyboard/LiveYield.h"

#include "galleon/keyboard/KeyboardLink.h"

#include <algorithm>
#include <cstdint>
#include <optional>

namespace galleon::keyboard {

void ReplyTracker::expect(std::uint8_t command)
{
    m_waiting = command;
}

void ReplyTracker::timedOut()
{
    if (!m_waiting)
        return;
    if (m_stale.size() >= MaxStale)
        m_stale.erase(m_stale.begin());
    m_stale.push_back(*m_waiting);
    m_waiting.reset();
}

void ReplyTracker::cancel()
{
    m_waiting.reset();
}

AnswerState answerStateAfter(LinkError error)
{
    switch (error) {
    case LinkError::Timeout: return AnswerState::MayComeLate;
    case LinkError::InvalidReply:
    case LinkError::SessionExpired: return AnswerState::Received;
    case LinkError::None:
    case LinkError::InvalidValue:
    case LinkError::WriteFailed:
    case LinkError::Disconnected:
    case LinkError::Unconfirmed:
    case LinkError::StaleReports: return AnswerState::NeverSent;
    }
    return AnswerState::NeverSent;
}

ReplyKind ReplyTracker::classify(std::uint8_t command)
{
    if (m_waiting && *m_waiting == command) {
        m_waiting.reset();
        return ReplyKind::Ours;
    }
    if (const auto stale = std::ranges::find(m_stale, command); stale != m_stale.end()) {
        m_stale.erase(stale); // one late answer per timed-out command
        return ReplyKind::Stale;
    }
    return ReplyKind::Foreign;
}

YieldPolicy::YieldPolicy(LiveClock::time_point connected)
    : m_listenFrom(connected), m_lastPoll(connected), m_lastForeign(connected)
{
}

YieldAction YieldPolicy::tick(LiveClock::time_point now)
{
    switch (m_state) {
    case LiveState::Listening:
        if (now - m_listenFrom >= ListenFor) {
            m_state = LiveState::Holding;
            m_lastPoll = now;
            return YieldAction::StartHolding;
        }
        return YieldAction::None;
    case LiveState::Holding:
        if (now - m_lastPoll >= ModePollEvery) {
            m_lastPoll = now;
            return YieldAction::ReadMode;
        }
        return YieldAction::None;
    case LiveState::Yielded:
        if (!m_probing && now - m_lastForeign >= QuietFor) {
            m_probing = true;
            return YieldAction::ReadMode;
        }
        return YieldAction::None;
    }
    return YieldAction::None;
}

void YieldPolicy::foreignTraffic(LiveClock::time_point now)
{
    m_state = LiveState::Yielded;
    m_lastForeign = now;
    m_probing = false; // a probe in flight is void: the other host is still there
}

YieldAction YieldPolicy::modeRead(std::uint8_t mode, LiveClock::time_point now)
{
    if (m_state == LiveState::Holding) {
        if (mode != 2)
            foreignTraffic(now); // someone else took the keyboard
        return YieldAction::None;
    }
    if (m_state == LiveState::Yielded && m_probing) {
        m_probing = false;
        if (mode == 1 || mode == 2) {
            // 1: the other host left. 2: a leftover frame of its own. Either way the keyboard is released and retaken.
            m_state = LiveState::Holding;
            m_lastPoll = now;
            return YieldAction::ReleaseAndResume;
        }
        m_lastForeign = now; // 3, 4, 5 or anything else: not ours to touch, wait another quiet period
    }
    return YieldAction::None;
}

StartDecision startDecision(bool heardAnotherHost, std::optional<std::uint8_t> mode)
{
    if (heardAnotherHost)
        return StartDecision::StartYielded;
    return mode == 1 ? StartDecision::Proceed : StartDecision::ReleaseFirst;
}

bool watchdogShouldRelease(std::optional<std::uint8_t> mode)
{
    return mode == 2;
}

} // namespace galleon::keyboard
