// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: LGPL-3.0-or-later

// The live lighting link over a fake keyboard that behaves like the one of the capture in docs/protocol/README.md
// "Live per-key lighting (Windows, 2026-10-25)": a mode, a session, a live file that must be open, frames that arrive
// in four reports, and a Web Hub that can speak or take the mode. The clock is an argument and the timeouts are
// short. Never opens real hardware, and nothing here proves the behaviour of a real keyboard.

#include "FakeLiveKeyboard.h"

#include <galleon/Transport.h>
#include <galleon/keyboard/LiveCommandGuard.h>
#include <galleon/keyboard/LiveFrame.h>
#include <galleon/keyboard/LiveLink.h>
#include <galleon/keyboard/LiveRequests.h>

#include <QTest>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>
#include <set>
#include <span>
#include <vector>

using namespace galleon::keyboard;
using namespace std::chrono_literals;

namespace {

const LiveClock::time_point t0 = LiveClock::now();
LiveClock::time_point at(std::chrono::milliseconds after) { return t0 + after; }

using fakelive::FakeDevice;

LiveLink::Settings settings()
{
    LiveLink::Settings s;
    s.timeout = 20ms;
    s.sessionRandom = {0x11, 0x22, 0x33, 0x44};
    return s;
}

LiveFrame f1Green()
{
    const std::vector<LedColor> leds{{0x3a, 0, 255, 0}};
    return buildLiveFrame(leds).value_or(LiveFrame{});
}

// A link that listened for 3 s in silence and took the keys: holding, at t = 3 s.
struct Holding
{
    FakeDevice device;
    LiveLink link{device, settings()};
    Holding()
    {
        link.begin(t0);
        link.tick(at(3000ms));
    }
};

using Commands = std::vector<std::uint8_t>;

} // namespace

class TestKeyboardLiveLink : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    // ---- starting -----------------------------------------------------------------------------------
    void nothingIsSentForThreeSecondsAfterBegin()
    {
        FakeDevice device;
        LiveLink link(device, settings());
        link.begin(t0);
        for (const auto ms : {0ms, 500ms, 1000ms, 2000ms, 2999ms})
            link.tick(at(ms));
        QVERIFY(device.sent.empty());
        QCOMPARE(link.status(), LiveLink::Status::Listening);
        QVERIFY(!link.keysTaken());
    }

    void afterThreeSecondsOfSilenceTheKeysAreTakenInTheOrderOfTheCapture()
    {
        FakeDevice device;
        LiveLink link(device, settings());
        link.begin(t0);
        link.tick(at(3000ms));
        QCOMPARE(device.commands(), (Commands{CommandSessionStart, CommandGet, CommandSet, CommandOpenFile}));
        QCOMPARE(device.mode, std::uint8_t{2});
        QVERIFY(device.fileOpen);
        QCOMPARE(link.status(), LiveLink::Status::Holding);
        QVERIFY(link.keysTaken());
        QCOMPARE(link.generation(), std::uint32_t{1});
    }

    void aLeftoverHostModeIsReleasedBeforeTheKeysAreTaken()
    {
        FakeDevice device;
        device.mode = 2; // a crash, a sleep or a kill left the keyboard in host mode
        LiveLink link(device, settings());
        link.begin(t0);
        link.tick(at(3000ms));
        // session, mode read (2: not a confirmed 1), then unbind, mode 1, read back, and only then mode 2 and the file
        QCOMPARE(device.commands(), (Commands{CommandSessionStart, CommandGet, CommandUnbind, CommandSet, CommandGet, CommandSet, CommandOpenFile}));
        QCOMPARE(device.mode, std::uint8_t{2});
        QVERIFY(link.keysTaken());
    }

    void aModeThatCannotBeReadIsNotAConfirmedOneSoTheKeyboardIsReleasedFirst()
    {
        FakeDevice device;
        device.unreadableMode = true;
        LiveLink link(device, settings());
        link.begin(t0);
        link.tick(at(3000ms));
        const Commands sent = device.commands();
        const auto unbind = std::find(sent.begin(), sent.end(), CommandUnbind);
        QVERIFY(unbind != sent.end()); // released before it was taken
        QCOMPARE(std::count(sent.begin(), sent.end(), CommandOpenFile), 1);
        QVERIFY(unbind < std::find(sent.begin(), sent.end(), CommandOpenFile));
    }

    void anotherHostHeardWhileListeningMeansNothingIsEverSent()
    {
        FakeDevice device;
        LiveLink link(device, settings());
        link.begin(t0);
        device.otherHostSpeaks();
        link.tick(at(1000ms));
        QCOMPARE(link.status(), LiveLink::Status::Yielded);
        for (const auto ms : {3000ms, 10000ms, 30000ms, 59000ms})
            link.tick(at(ms));
        QVERIFY(device.sent.empty());
        QCOMPARE(link.show(f1Green(), at(5000ms)), LiveLink::ShowResult::NotHolding);
        QVERIFY(device.sent.empty());
    }

    void aKeyboardThatNeverStopsTalkingIsTakenForAnotherHostAndNothingIsSent()
    {
        FakeDevice device;
        device.spam = true;
        LiveLink link(device, settings());
        link.begin(t0);
        link.tick(at(1000ms));
        link.tick(at(3000ms));
        QCOMPARE(link.status(), LiveLink::Status::Yielded);
        QVERIFY(device.sent.empty());
    }

    // ---- frames -------------------------------------------------------------------------------------
    void aFrameGoesOutAsFourReportsAndTheKeyboardHasItWhole()
    {
        Holding h;
        const std::size_t before = h.device.sent.size();
        QCOMPARE(h.link.show(f1Green(), at(3100ms)), LiveLink::ShowResult::Sent);
        const Commands all = h.device.commands();
        QCOMPARE(Commands(all.begin() + static_cast<std::ptrdiff_t>(before), all.end()),
                 (Commands{CommandWriteBegin, CommandWriteMore, CommandWriteMore, CommandWriteMore}));
        QCOMPARE(h.device.framesComplete, 1);
        const LiveFrame expected = f1Green();
        QVERIFY(std::equal(expected.begin(), expected.end(), h.device.lastFrame.begin()));
    }

    void framesComingTooFastAreDroppedNotQueued()
    {
        Holding h;
        QCOMPARE(h.link.show(f1Green(), at(3100ms)), LiveLink::ShowResult::Sent);
        QCOMPARE(h.link.show(f1Green(), at(3100ms)), LiveLink::ShowResult::Sent); // the burst of two
        QCOMPARE(h.link.show(f1Green(), at(3100ms)), LiveLink::ShowResult::Dropped);
        QCOMPARE(h.device.framesComplete, 2);
        QCOMPARE(h.link.show(f1Green(), at(3200ms)), LiveLink::ShowResult::Sent); // a tenth of a second later: room again
    }

    void nothingIsShownBeforeTheKeysAreTaken()
    {
        FakeDevice device;
        LiveLink link(device, settings());
        QCOMPARE(link.show(f1Green(), at(0ms)), LiveLink::ShowResult::NotHolding); // not even begun
        link.begin(t0);
        QCOMPARE(link.show(f1Green(), at(1000ms)), LiveLink::ShowResult::NotHolding); // still listening
        QVERIFY(device.sent.empty());
    }

    // ---- giving way ---------------------------------------------------------------------------------
    void anAnswerNobodyAskedForMakesItYieldAndNothingMoreIsSentNotEvenARelease()
    {
        Holding h;
        const std::size_t before = h.device.sent.size();
        h.device.otherHostSpeaks();
        QCOMPARE(h.link.show(f1Green(), at(3100ms)), LiveLink::ShowResult::NotHolding);
        QCOMPARE(h.link.status(), LiveLink::Status::Yielded);
        h.link.tick(at(3500ms));
        QCOMPARE(h.device.sent.size(), before);
        QCOMPARE(h.link.release(at(4000ms)), ReleaseOutcome::NotNeeded); // an unbind could break the other host's transfer
        QCOMPARE(h.device.sent.size(), before);
    }

    void aModeThatIsNotTwoAtTheNextPollMeansAnotherHostTookTheKeyboard()
    {
        Holding h;
        h.device.webHubTakes(); // no traffic, only the mode changed
        h.link.tick(at(4000ms)); // a second after the start: the poll
        QCOMPARE(h.link.status(), LiveLink::Status::Yielded);
        const Commands sent = h.device.commands();
        QCOMPARE(std::count(sent.begin(), sent.end(), CommandUnbind), 0);
        QCOMPARE(h.link.show(f1Green(), at(4100ms)), LiveLink::ShowResult::NotHolding);
    }

    void afterSixtySecondsOfQuietItReleasesAndTakesTheKeysAgain()
    {
        Holding h;
        h.device.webHubTakes();
        h.link.tick(at(4000ms)); // yielded here
        const std::size_t sentAtYield = h.device.sent.size();
        h.link.tick(at(63000ms)); // 59 s later
        QCOMPARE(h.device.sent.size(), sentAtYield);
        h.link.tick(at(64000ms)); // 60 s of quiet: the probe finds mode 1, releases and takes the keys again
        QCOMPARE(h.link.status(), LiveLink::Status::Holding);
        QCOMPARE(h.device.mode, std::uint8_t{2});
        QVERIFY(h.device.fileOpen);
        QCOMPARE(h.link.generation(), std::uint32_t{2}); // a caller that keeps a scene sends it again
    }

    void aProbeThatFindsAnyModeButOneOrTwoLeavesTheKeyboardAlone()
    {
        Holding h;
        h.device.mode = 3; // a bootloader mode: never ours to touch
        h.link.tick(at(4000ms)); // yields
        const Commands before = h.device.commands();
        h.link.tick(at(64000ms)); // the probe reads the mode, and nothing else
        const Commands after = h.device.commands();
        QCOMPARE(Commands(after.begin() + static_cast<std::ptrdiff_t>(before.size()), after.end()), (Commands{CommandGet}));
        QCOMPARE(h.device.mode, std::uint8_t{3});
        QCOMPARE(h.link.status(), LiveLink::Status::Yielded);
    }

    // ---- failures -----------------------------------------------------------------------------------
    void twoFailedFramesInARowStopTheStreamForGoodAndGiveTheKeysBack()
    {
        Holding h;
        h.device.silentFrames = true;
        QCOMPARE(h.link.show(f1Green(), at(3100ms)), LiveLink::ShowResult::Failed);
        QCOMPARE(h.link.show(f1Green(), at(3300ms)), LiveLink::ShowResult::Frozen);
        QCOMPARE(h.link.status(), LiveLink::Status::Frozen);
        QCOMPARE(h.device.mode, std::uint8_t{1}); // the keys were given back as it stopped
        const std::size_t sent = h.device.sent.size();
        QCOMPARE(h.link.show(f1Green(), at(3600ms)), LiveLink::ShowResult::Frozen);
        h.link.tick(at(10000ms));
        QCOMPARE(h.device.sent.size(), sent);
    }

    void aFrameThatBrokeOffHalfwayDoesNotBlockTheNextOne()
    {
        Holding h;
        h.device.frameReportsBeforeSilence = 1; // the first report is answered, the second is not
        QCOMPARE(h.link.show(f1Green(), at(3100ms)), LiveLink::ShowResult::Failed);
        QCOMPARE(h.link.show(f1Green(), at(3300ms)), LiveLink::ShowResult::Sent); // a whole frame, from its beginning
        QCOMPARE(h.device.framesComplete, 1);
        QCOMPARE(h.link.status(), LiveLink::Status::Holding);
    }

    void aSuccessBetweenTwoFailuresKeepsTheStreamGoing()
    {
        Holding h;
        h.device.silentFrames = true;
        h.device.silentFramesTimes = 1; // only the first report of the first frame goes unanswered
        QCOMPARE(h.link.show(f1Green(), at(3100ms)), LiveLink::ShowResult::Failed);
        QCOMPARE(h.link.show(f1Green(), at(3300ms)), LiveLink::ShowResult::Sent);
        QCOMPARE(h.link.show(f1Green(), at(3500ms)), LiveLink::ShowResult::Sent);
        QCOMPARE(h.link.status(), LiveLink::Status::Holding);
    }

    void afterAReplugTheStreamCanStartOverFromTheBeginning()
    {
        Holding h;
        h.device.silentFrames = true;
        static_cast<void>(h.link.show(f1Green(), at(3100ms)));
        static_cast<void>(h.link.show(f1Green(), at(3300ms)));
        QCOMPARE(h.link.status(), LiveLink::Status::Frozen);
        h.device.silentFrames = false;
        h.link.resetAfterReplug();
        QCOMPARE(h.link.status(), LiveLink::Status::Idle);
        h.link.begin(at(20000ms));
        h.link.tick(at(23000ms));
        QCOMPARE(h.link.status(), LiveLink::Status::Holding);
        QVERIFY(h.link.keysTaken());
        QCOMPARE(h.link.show(f1Green(), at(23100ms)), LiveLink::ShowResult::Sent);
    }

    void aSessionThatExpiresIsStartedAgainAndTheKeysAreTakenAnew()
    {
        Holding h;
        h.device.expireNext = true;
        QCOMPARE(h.link.show(f1Green(), at(3100ms)), LiveLink::ShowResult::Failed);
        QCOMPARE(h.link.lastError(), LinkError::SessionExpired);
        QVERIFY(!h.link.keysTaken());
        h.link.tick(at(3200ms)); // the next tick takes them again, in a new session
        QVERIFY(h.link.keysTaken());
        QCOMPARE(h.link.generation(), std::uint32_t{2});
        QCOMPARE(h.link.show(f1Green(), at(3400ms)), LiveLink::ShowResult::Sent);
    }

    void aKeyboardThatIsGoneStopsEverything()
    {
        Holding h;
        h.device.disconnected = true;
        h.link.tick(at(4000ms));
        QCOMPARE(h.link.status(), LiveLink::Status::Disconnected);
        const std::size_t sent = h.device.sent.size();
        QCOMPARE(h.link.show(f1Green(), at(4100ms)), LiveLink::ShowResult::NotHolding);
        QCOMPARE(h.device.sent.size(), sent);
    }

    // ---- giving the keys back -----------------------------------------------------------------------
    void releaseGivesTheKeysBackAndSaysSo()
    {
        Holding h;
        const std::size_t before = h.device.sent.size();
        QCOMPARE(h.link.release(at(3500ms)), ReleaseOutcome::Released);
        const Commands all = h.device.commands();
        QCOMPARE(Commands(all.begin() + static_cast<std::ptrdiff_t>(before), all.end()), (Commands{CommandUnbind, CommandSet, CommandGet}));
        QCOMPARE(h.device.mode, std::uint8_t{1});
        QVERIFY(!h.device.fileOpen);
        QCOMPARE(h.link.status(), LiveLink::Status::Released);
        QCOMPARE(h.link.show(f1Green(), at(3600ms)), LiveLink::ShowResult::NotHolding);
    }

    void releaseWhileStillListeningTouchesNothing()
    {
        FakeDevice device;
        LiveLink link(device, settings());
        link.begin(t0);
        QCOMPARE(link.release(at(1000ms)), ReleaseOutcome::NotNeeded);
        QVERIFY(device.sent.empty());
    }

    // ---- the allowlist ------------------------------------------------------------------------------
    void everyReportThatLeftIsOneTheAllowlistLetsThrough()
    {
        Holding h;
        static_cast<void>(h.link.show(f1Green(), at(3100ms)));
        static_cast<void>(h.link.show(f1Green(), at(3300ms)));
        h.link.tick(at(4000ms));
        static_cast<void>(h.link.release(at(5000ms)));
        LiveCommandGuard guard;
        QVERIFY(h.device.sent.size() > 10);
        for (const auto &report : h.device.sent) {
            QCOMPARE(guard.check(report), Refusal::None);
            guard.confirmed(report); // every answer of the fake was status 0
        }
    }

    void theLinkNeverSetsAModeButOneOrTwoOrWritesAnotherProperty()
    {
        Holding h;
        h.device.webHubTakes();
        h.link.tick(at(4000ms));
        h.link.tick(at(64000ms));
        static_cast<void>(h.link.release(at(65000ms)));
        for (const auto &report : h.device.sent) {
            if (report[4] == CommandSet) {
                QCOMPARE(report[5], PropertyMode);
                QVERIFY(report[7] == 1 || report[7] == 2);
            }
        }
    }
};

QTEST_GUILESS_MAIN(TestKeyboardLiveLink)
#include "test_keyboardlivelink.moc"
