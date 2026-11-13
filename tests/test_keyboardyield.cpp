// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: LGPL-3.0-or-later

// Giving way to another controller: classification of answers, the yield policy and the start and watchdog
// decisions. Pure code, time is an argument: nothing waits and nothing is sent. The numbers (3 s listening,
// 1 s mode poll, 60 s quiet) are the ones measured by Aaryush1 (docs/WINDOWS_LIVE_LIGHTING_CSHARP.md).

#include <galleon/keyboard/LiveYield.h>

#include <QTest>

using namespace galleon::keyboard;
using namespace std::chrono_literals;

namespace {

const LiveClock::time_point t0 = LiveClock::now();

LiveClock::time_point at(std::chrono::milliseconds after) { return t0 + after; }

constexpr std::uint8_t GetCommand = 0x02;
constexpr std::uint8_t PingCommand = 0x12;

// A policy that has listened in silence and now holds the keys.
YieldPolicy holding()
{
    YieldPolicy policy(t0);
    [[maybe_unused]] const auto started = policy.tick(at(3000ms));
    return policy;
}

// A policy that is yielded because of foreign traffic at `when`.
YieldPolicy yielded(std::chrono::milliseconds when)
{
    YieldPolicy policy = holding();
    policy.foreignTraffic(at(when));
    return policy;
}

} // namespace

class TestKeyboardYield : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    // ---- classification -----------------------------------------------------------------------------
    void theAnswerToTheCommandBeingWaitedForIsOurs()
    {
        ReplyTracker tracker;
        tracker.expect(GetCommand);
        QCOMPARE(tracker.classify(GetCommand), ReplyKind::Ours);
    }

    void anAnswerWhenNothingIsWaitedForIsForeign()
    {
        ReplyTracker tracker;
        QCOMPARE(tracker.classify(GetCommand), ReplyKind::Foreign);
    }

    void anAnswerToAnotherCommandThanTheOneWaitedForIsForeign()
    {
        ReplyTracker tracker;
        tracker.expect(GetCommand);
        QCOMPARE(tracker.classify(PingCommand), ReplyKind::Foreign);
    }

    void aLateAnswerToOurOwnTimedOutCommandIsStaleOnceThenForeign()
    {
        ReplyTracker tracker;
        tracker.expect(GetCommand);
        tracker.timedOut();
        QCOMPARE(tracker.classify(GetCommand), ReplyKind::Stale); // late, but ours: not another host
        QCOMPARE(tracker.classify(GetCommand), ReplyKind::Foreign); // a second one is not
    }

    void anAnswerIsNotOursOnceItWasAlreadyReceived()
    {
        ReplyTracker tracker;
        tracker.expect(GetCommand);
        QCOMPARE(tracker.classify(GetCommand), ReplyKind::Ours);
        QCOMPARE(tracker.classify(GetCommand), ReplyKind::Foreign);
    }

    void theListOfTimedOutCommandsStaysBounded()
    {
        ReplyTracker tracker;
        for (std::size_t i = 0; i <= ReplyTracker::MaxStale; ++i) {
            tracker.expect(static_cast<std::uint8_t>(0x20 + i));
            tracker.timedOut();
        }
        QCOMPARE(tracker.classify(0x20), ReplyKind::Foreign); // the oldest was forgotten
        QCOMPARE(tracker.classify(static_cast<std::uint8_t>(0x20 + ReplyTracker::MaxStale)), ReplyKind::Stale);
    }

    // ---- listening and holding ----------------------------------------------------------------------
    void nothingIsSentForThreeSecondsAfterConnecting()
    {
        YieldPolicy policy(t0);
        QCOMPARE(policy.state(), LiveState::Listening);
        QVERIFY(!policy.mayWrite());
        QCOMPARE(policy.tick(at(0ms)), YieldAction::None);
        QCOMPARE(policy.tick(at(2999ms)), YieldAction::None);
        QVERIFY(!policy.mayWrite());
        QCOMPARE(policy.tick(at(3000ms)), YieldAction::StartHolding);
        QCOMPARE(policy.state(), LiveState::Holding);
        QVERIFY(policy.mayWrite());
    }

    void anotherHostHeardWhileListeningMeansStartingYielded()
    {
        YieldPolicy policy(t0);
        policy.foreignTraffic(at(1200ms));
        QCOMPARE(policy.state(), LiveState::Yielded);
        QCOMPARE(policy.tick(at(3000ms)), YieldAction::None); // never StartHolding
        QVERIFY(!policy.mayWrite());
    }

    void theModeIsReadAboutOnceASecondWhileHolding()
    {
        YieldPolicy policy = holding(); // holding from 3 s
        QCOMPARE(policy.tick(at(3500ms)), YieldAction::None);
        QCOMPARE(policy.tick(at(4000ms)), YieldAction::ReadMode);
        QCOMPARE(policy.tick(at(4500ms)), YieldAction::None);
        QCOMPARE(policy.tick(at(5000ms)), YieldAction::ReadMode);
    }

    void aModeOfTwoKeepsTheKeys()
    {
        YieldPolicy policy = holding();
        QCOMPARE(policy.modeRead(2, at(4000ms)), YieldAction::None);
        QCOMPARE(policy.state(), LiveState::Holding);
    }

    void aModeThatIsNotTwoMeansSomeoneElseTookTheKeyboard()
    {
        for (const std::uint8_t mode : std::initializer_list<std::uint8_t>{0, 1, 3, 4, 5}) {
            YieldPolicy policy = holding();
            QCOMPARE(policy.modeRead(mode, at(4000ms)), YieldAction::None);
            QCOMPARE(policy.state(), LiveState::Yielded);
            QVERIFY(!policy.mayWrite());
        }
    }

    // ---- yielded ------------------------------------------------------------------------------------
    void nothingIsReleasedWhileYielded()
    {
        YieldPolicy policy = holding();
        QVERIFY(policy.mayRelease());
        policy.foreignTraffic(at(4000ms));
        QVERIFY(!policy.mayRelease()); // an unbind could break the other host's file transfer
    }

    void nothingAtAllIsAskedForSixtySecondsOfQuiet()
    {
        YieldPolicy policy = yielded(10000ms);
        for (int seconds = 11; seconds < 70; ++seconds)
            QCOMPARE(policy.tick(at(std::chrono::seconds(seconds))), YieldAction::None);
        QCOMPARE(policy.tick(at(70s)), YieldAction::ReadMode); // 60 s after the last foreign traffic
    }

    void theProbeIsAskedOnceNotOnEveryTick()
    {
        YieldPolicy policy = yielded(0ms);
        QCOMPARE(policy.tick(at(60s)), YieldAction::ReadMode);
        QCOMPARE(policy.tick(at(61s)), YieldAction::None);
        QCOMPARE(policy.tick(at(100s)), YieldAction::None);
    }

    void foreignTrafficRestartsTheQuietPeriod()
    {
        YieldPolicy policy = yielded(0ms);
        policy.foreignTraffic(at(50s)); // the other host is still polling
        QCOMPARE(policy.tick(at(60s)), YieldAction::None);
        QCOMPARE(policy.tick(at(109s)), YieldAction::None);
        QCOMPARE(policy.tick(at(110s)), YieldAction::ReadMode);
    }

    void aProbeThatFindsModeOneOrTwoReleasesAndResumes()
    {
        for (const std::uint8_t mode : std::initializer_list<std::uint8_t>{1, 2}) {
            YieldPolicy policy = yielded(0ms);
            QCOMPARE(policy.tick(at(60s)), YieldAction::ReadMode);
            QCOMPARE(policy.modeRead(mode, at(60s)), YieldAction::ReleaseAndResume);
            QCOMPARE(policy.state(), LiveState::Holding);
            QVERIFY(policy.mayWrite());
        }
    }

    void aProbeThatFindsAnyOtherModeStaysYieldedForAnotherQuietPeriod()
    {
        for (const std::uint8_t mode : std::initializer_list<std::uint8_t>{3, 4, 5, 0}) { // bootloaders, sleep: not ours to touch
            YieldPolicy policy = yielded(0ms);
            QCOMPARE(policy.tick(at(60s)), YieldAction::ReadMode);
            QCOMPARE(policy.modeRead(mode, at(60s)), YieldAction::None);
            QCOMPARE(policy.state(), LiveState::Yielded);
            QCOMPARE(policy.tick(at(119s)), YieldAction::None);
            QCOMPARE(policy.tick(at(120s)), YieldAction::ReadMode);
        }
    }

    void foreignTrafficDuringTheProbeVoidsIt()
    {
        YieldPolicy policy = yielded(0ms);
        QCOMPARE(policy.tick(at(60s)), YieldAction::ReadMode);
        policy.foreignTraffic(at(60500ms));
        QCOMPARE(policy.modeRead(1, at(61s)), YieldAction::None); // the answer of a void probe is not acted on
        QCOMPARE(policy.state(), LiveState::Yielded);
    }

    void aModeReadNobodyAskedForDoesNotLiftTheYield()
    {
        YieldPolicy policy = yielded(0ms);
        QCOMPARE(policy.modeRead(1, at(5s)), YieldAction::None);
        QCOMPARE(policy.state(), LiveState::Yielded);
    }

    void theMeasuredScenarioOfTheWebHubOpenedMidRun()
    {
        // Yielded on its first reply, nothing sent for as long as it polled, resumed 60 s after its last traffic.
        YieldPolicy policy = holding();
        policy.foreignTraffic(at(20s));
        for (auto seconds = 21s; seconds < 40s; seconds += 2s) {
            policy.foreignTraffic(at(seconds)); // the Web Hub polls every ~2.5 s
            QCOMPARE(policy.tick(at(seconds)), YieldAction::None);
        }
        const auto lastTraffic = 39s;
        QCOMPARE(policy.tick(at(lastTraffic + 59s)), YieldAction::None);
        QCOMPARE(policy.tick(at(lastTraffic + 60s)), YieldAction::ReadMode);
        QCOMPARE(policy.modeRead(1, at(lastTraffic + 60s)), YieldAction::ReleaseAndResume);
    }

    // ---- start and watchdog -------------------------------------------------------------------------
    void aConfirmedModeOneIsLeftAlone()
    {
        QCOMPARE(startDecision(false, 1), StartDecision::Proceed);
    }

    void anythingButAConfirmedModeOneIsReleasedFirst()
    {
        for (const std::uint8_t mode : std::initializer_list<std::uint8_t>{0, 2, 3, 4, 5})
            QCOMPARE(startDecision(false, mode), StartDecision::ReleaseFirst);
        QCOMPARE(startDecision(false, std::nullopt), StartDecision::ReleaseFirst); // an unreadable mode is not a confirmed 1
    }

    void anotherHostAlreadyTalkingMeansNothingIsSentAndNothingReleased()
    {
        for (const std::optional<std::uint8_t> mode : {std::optional<std::uint8_t>{1}, std::optional<std::uint8_t>{2}, std::optional<std::uint8_t>{}})
            QCOMPARE(startDecision(true, mode), StartDecision::StartYielded);
    }

    void theWatchdogReleasesOnlyAKeyboardStillInHostMode()
    {
        QVERIFY(watchdogShouldRelease(2));
        QVERIFY(!watchdogShouldRelease(1)); // the writer had released already
        for (const std::uint8_t mode : std::initializer_list<std::uint8_t>{0, 3, 4, 5})
            QVERIFY(!watchdogShouldRelease(mode));
        QVERIFY(!watchdogShouldRelease(std::nullopt));
    }
};

QTEST_GUILESS_MAIN(TestKeyboardYield)
#include "test_keyboardyield.moc"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
