// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "galleon/keyboard/LiveLink.h"

#include "CheckedLink.h"

#include "galleon/keyboard/BragiReply.h"
#include "galleon/keyboard/LiveCommandGuard.h"
#include "galleon/keyboard/LiveRequests.h"

#include <array>
#include <optional>

namespace galleon::keyboard {

namespace {
constexpr std::size_t SessionIdAt = 9; // the answer to a session start: `1b 00 rr rr rr rr <session>`
constexpr std::size_t CommandAt = 3;
constexpr int MaxUnsolicited = 64; // bounded: a keyboard that never stops talking cannot hold the caller
constexpr std::uint8_t UnreadableMode = 0xff; // not 1, not 2: a probe that could not read counts as "still not ours"
} // namespace

struct LiveLink::Impl
{
    Impl(LiveLink &parent, Transport &device, const Settings &options)
        : owner(parent), transport(device), settings(options), policy(LiveClock::now()), limiter(LiveClock::now()),
          link(device, options.timeout, [this](std::span<const std::uint8_t> report) { classify(report); })
    {
    }

    LiveLink &owner;
    Transport &transport;
    Settings settings;
    YieldPolicy policy;
    ReplyTracker tracker;
    RateLimiter limiter;
    FreezePolicy freeze;
    detail::CheckedLink link;
    LiveClock::time_point now;
    std::uint8_t session = 0;
    bool begun = false;
    bool released = false;
    bool gone = false;

    // An answer that is not the one being waited for: another host's, unless it is a late answer of ours.
    void classify(std::span<const std::uint8_t> report)
    {
        const auto payload = detail::answerPayload(report);
        if (!payload)
            return; // not a Bragi answer: nothing to conclude from it
        if (tracker.classify((*payload)[CommandAt]) == ReplyKind::Foreign)
            policy.foreignTraffic(now);
    }

    // Reads whatever is waiting before anything is sent: with nothing awaited, every answer is foreign.
    void drainUnsolicited()
    {
        std::array<std::uint8_t, detail::ReadBufferSize> buffer{};
        for (int i = 0; i < MaxUnsolicited; ++i) {
            const int n = transport.read(buffer);
            if (n < 0) {
                gone = true;
                return;
            }
            if (n == 0)
                return;
            classify(std::span<const std::uint8_t>(buffer).first(static_cast<std::size_t>(n)));
        }
    }

    std::optional<std::vector<std::uint8_t>> send(const Request &request)
    {
        drainUnsolicited();
        if (gone) {
            owner.m_lastError = LinkError::Disconnected;
            return std::nullopt;
        }
        tracker.expect(request[4]);
        auto answer = link.send(request);
        if (answer) {
            static_cast<void>(tracker.classify(request[4])); // our own answer: nothing waited for any more
        } else {
            owner.m_lastError = link.error();
            switch (answerStateAfter(link.error())) {
            case AnswerState::MayComeLate: tracker.timedOut(); break;
            case AnswerState::Received: static_cast<void>(tracker.classify(request[4])); break; // came, with an error: nothing late to come
            case AnswerState::NeverSent: tracker.cancel(); break;
            }
            if (link.error() == LinkError::SessionExpired) {
                session = 0; // a new one is started at the next request, and the keys are taken anew at the next tick
                owner.m_taken = false;
                link.guard().reset();
            }
        }
        return answer;
    }

    bool ensureSession()
    {
        if (session != 0)
            return true;
        const auto answer = send(sessionStartRequest(settings.sessionRandom));
        if (!answer)
            return false;
        session = (*answer)[SessionIdAt];
        return session != 0;
    }

    std::optional<std::uint8_t> readMode()
    {
        if (!ensureSession())
            return std::nullopt;
        const auto answer = send(getPropertyIn(session, PropertyMode));
        if (!answer)
            return std::nullopt;
        const auto parsed = parsePropertyReply(*answer);
        if (!parsed || parsed->command != CommandGet || parsed->value > 0xff)
            return std::nullopt;
        return static_cast<std::uint8_t>(parsed->value);
    }

    bool setMode(std::uint8_t mode)
    {
        return ensureSession() && send(setModeRequest(session, mode)).has_value();
    }

    // Unbind, mode 1, read back; each step is tried even if the one before failed.
    ReleaseOutcome releaseSequence()
    {
        if (!ensureSession())
            return ReleaseOutcome::Failed;
        static_cast<void>(send(unbindRequest(session)));
        static_cast<void>(setMode(ModeSelfOperated));
        const auto after = readMode();
        link.guard().reset();
        owner.m_taken = false;
        return after == ModeSelfOperated ? ReleaseOutcome::Released : ReleaseOutcome::Failed;
    }

    void failure()
    {
        freeze.failure();
        if (freeze.tripped()) {
            static_cast<void>(releaseSequence()); // best effort: the keys should not stay as they were
            owner.m_taken = false;
        }
    }

    // Another host may have spoken during the exchange before this one (the Web Hub connecting in the middle of a takeover):
    // what is heard is classified at once, and a takeover never goes on over it. False also when the keyboard is gone.
    bool stillOurs()
    {
        drainUnsolicited();
        return !gone && policy.state() == LiveState::Holding;
    }

    // Each step that writes is preceded by the check above, so that a host that connects mid-takeover is overridden by
    // nothing more than the step that was already on its way. When it speaks after the mode was set, the keys are left as
    // they are (nothing is released over another host's traffic) and the file is not opened: the link is yielded and
    // resumes by the usual rule after the quiet period. Not a failure of the link: the freeze count is untouched.
    bool takeKeys()
    {
        const auto mode = readMode();
        if (!stillOurs())
            return false;
        // A leftover of a crash, a sleep or a kill is released first; only a CONFIRMED 1 is left alone (a mode that
        // could not be read is not one).
        if (startDecision(false, mode) == StartDecision::ReleaseFirst) {
            static_cast<void>(releaseSequence());
            if (!stillOurs())
                return false;
        }
        if (!setMode(ModeHostControlled)) {
            failure();
            return false;
        }
        if (!stillOurs())
            return false;
        if (!send(openLiveFileRequest(session))) {
            failure();
            return false;
        }
        owner.m_taken = true;
        ++owner.m_generation;
        freeze.success();
        return true;
    }

    // The mode, read while holding or as the probe after the quiet period.
    void pollMode(bool probing)
    {
        const auto mode = readMode();
        if (!mode) {
            if (probing)
                static_cast<void>(policy.modeRead(UnreadableMode, now)); // still not ours: wait another quiet period
            else
                failure();
            return;
        }
        if (!probing)
            freeze.success();
        if (policy.modeRead(*mode, now) == YieldAction::ReleaseAndResume) {
            static_cast<void>(releaseSequence());
            static_cast<void>(takeKeys());
        }
    }
};

LiveLink::LiveLink(Transport &transport, Settings settings) : m_impl(std::make_unique<Impl>(*this, transport, settings)) {}

LiveLink::~LiveLink() = default;

void LiveLink::begin(LiveClock::time_point now)
{
    m_impl->policy = YieldPolicy(now);
    m_impl->limiter = RateLimiter(now);
    m_impl->begun = true;
    m_impl->released = false;
    m_impl->now = now;
}

void LiveLink::tick(LiveClock::time_point now)
{
    Impl &s = *m_impl;
    if (!s.begun || s.released || s.freeze.tripped() || s.gone)
        return;
    s.now = now;
    s.drainUnsolicited();
    if (s.gone)
        return;
    const YieldAction action = s.policy.tick(now);
    switch (s.policy.state()) {
    case LiveState::Listening:
        break;
    case LiveState::Holding:
        if (action == YieldAction::StartHolding || !m_taken) {
            static_cast<void>(s.takeKeys()); // the first start, or a start that failed and is tried again
        } else if (action == YieldAction::ReadMode) {
            s.pollMode(false);
        }
        break;
    case LiveState::Yielded:
        if (action == YieldAction::ReadMode)
            s.pollMode(true);
        break;
    }
}

LiveLink::ShowResult LiveLink::show(const LiveFrame &frame, LiveClock::time_point now)
{
    Impl &s = *m_impl;
    if (s.freeze.tripped())
        return ShowResult::Frozen;
    if (!s.begun || s.released || s.gone)
        return ShowResult::NotHolding;
    s.now = now;
    s.drainUnsolicited(); // another host may have spoken since the last tick
    if (s.policy.state() != LiveState::Holding || !m_taken)
        return ShowResult::NotHolding;
    if (!s.limiter.tryTake(now))
        return ShowResult::Dropped;
    const auto reports = liveFrameReports(frame, s.session);
    if (!reports)
        return ShowResult::NotHolding; // no session: the keys are not really ours
    for (const auto &report : *reports) {
        if (!s.send(report)) {
            s.link.guard().abortFrame(); // the rest of this frame will never come: the next frame starts over
            s.failure();
            return s.freeze.tripped() ? ShowResult::Frozen : ShowResult::Failed;
        }
    }
    s.freeze.success();
    return ShowResult::Sent;
}

ReleaseOutcome LiveLink::release(LiveClock::time_point now)
{
    Impl &s = *m_impl;
    if (!s.begun || s.released || s.gone)
        return ReleaseOutcome::NotNeeded;
    s.now = now;
    s.drainUnsolicited();
    if (s.policy.state() == LiveState::Yielded || s.policy.state() == LiveState::Listening)
        return ReleaseOutcome::NotNeeded; // the keys are not ours to give back
    const ReleaseOutcome outcome = s.releaseSequence();
    s.released = true;
    return outcome;
}

void LiveLink::resetAfterReplug()
{
    Impl &s = *m_impl;
    s.freeze.reset();
    s.link.guard().reset();
    s.session = 0;
    s.begun = false;
    s.released = false;
    s.gone = false;
    m_taken = false;
    m_lastError = LinkError::None;
}

LiveLink::Status LiveLink::status() const
{
    const Impl &s = *m_impl;
    if (!s.begun)
        return Status::Idle;
    if (s.released)
        return Status::Released;
    if (s.gone)
        return Status::Disconnected;
    if (s.freeze.tripped())
        return Status::Frozen;
    switch (s.policy.state()) {
    case LiveState::Listening: return Status::Listening;
    case LiveState::Holding: return Status::Holding;
    case LiveState::Yielded: return Status::Yielded;
    }
    return Status::Idle;
}

const char *describe(LiveLink::Status status)
{
    switch (status) {
    case LiveLink::Status::Idle: return "not started";
    case LiveLink::Status::Listening: return "listening for another host";
    case LiveLink::Status::Holding: return "holding the keys";
    case LiveLink::Status::Yielded: return "yielded to another host";
    case LiveLink::Status::Frozen: return "stopped after two failures";
    case LiveLink::Status::Disconnected: return "keyboard gone";
    case LiveLink::Status::Released: return "keys given back";
    }
    return "unknown";
}

} // namespace galleon::keyboard
