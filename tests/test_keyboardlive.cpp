// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: LGPL-3.0-or-later

// Safety layer of the keyboard's live lighting: allowlist, frame layout, pacing and freeze policy. Pure code,
// nothing is sent. The byte vectors come from the capture in docs/protocol/README.md "Live per-key lighting
// (Windows, 2026-10-25)" (Aaryush1, issue #56).

#include <galleon/keyboard/BragiFrame.h>
#include <galleon/keyboard/LiveCommandGuard.h>
#include <galleon/keyboard/LiveFrame.h>
#include <galleon/keyboard/LivePolicy.h>

#include <QTest>

#include <array>
#include <vector>

using namespace galleon::keyboard;
using namespace std::chrono_literals;

namespace {

using Report = std::array<std::uint8_t, WriteSize>;

// A request: report ID 00, direction 00, request marker 01, then `session, command, payload...` as the
// capture lists them, zero padded.
Report report(std::initializer_list<std::uint8_t> bytes)
{
    Report r{};
    r[2] = 0x01;
    std::size_t at = 3;
    for (const std::uint8_t b : bytes)
        r.at(at++) = b;
    return r;
}

Report sessionStart() { return report({0x00, 0x1b, 0x01, 0x11, 0x22, 0x33, 0x44}); }
Report setMode(std::uint8_t mode) { return report({0x01, 0x01, 0x03, 0x00, mode, 0x00}); }
Report openLive() { return report({0x01, 0x0d, 0x00, 0x22, 0x00}); }
Report unbind() { return report({0x01, 0x05, 0x01, 0x00}); }

LiveFrame frameOf(const std::vector<LedColor> &leds)
{
    return buildLiveFrame(leds).value_or(LiveFrame{});
}

LiveReports f1Green()
{
    return liveFrameReports(frameOf({{0x3a, 0, 255, 0}}), 1).value_or(LiveReports{});
}

// Confirms every report of a frame after checking it.
void sendFrame(LiveCommandGuard &guard, const LiveReports &frame)
{
    for (const Report &r : frame) {
        QCOMPARE(guard.check(r), Refusal::None);
        guard.confirmed(r);
    }
}

LiveCommandGuard openGuard()
{
    LiveCommandGuard guard;
    guard.confirmed(openLive());
    return guard;
}

} // namespace

class TestKeyboardLive : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    // ---- frame layout -------------------------------------------------------------------------------
    void theWorkedExampleOfTheCaptureIsReproducedByteForByte()
    {
        const LiveReports r = f1Green();
        // First report: `00 01 01 06 | 00 | b8 01 00 00 | 12 00 ...` after the report ID.
        const std::array<std::uint8_t, 11> head{0x00, 0x00, 0x01, 0x01, 0x06, 0x00, 0xb8, 0x01, 0x00, 0x00, 0x12};
        QVERIFY(std::equal(head.begin(), head.end(), r[0].begin()));
        QCOMPARE(r[1][4], CommandWriteMore);
        QCOMPARE(r[1][64], 0xff); // F1's green byte: frame byte 177, byte 63 of the second report
        std::size_t greens = 0;
        for (const Report &one : r)
            greens += static_cast<std::size_t>(std::count(one.begin() + 5, one.end(), std::uint8_t{0xff}));
        QCOMPARE(greens, std::size_t{1});
    }

    void theFourReportsCarryTheWholeFrameAndNothingMore()
    {
        std::vector<LedColor> leds;
        for (std::uint16_t i = 0; i < LiveLedCount; ++i)
            leds.push_back({i, static_cast<std::uint8_t>(i + 1), static_cast<std::uint8_t>(i + 2), static_cast<std::uint8_t>(i + 3)});
        QVERIFY(buildLiveFrame(leds));
        const LiveFrame frame = frameOf(leds);
        QVERIFY(liveFrameReports(frame, 7));
        const LiveReports reports = liveFrameReports(frame, 7).value_or(LiveReports{});
        std::vector<std::uint8_t> joined;
        for (std::size_t i = 0; i < reports.size(); ++i) {
            const auto &one = reports.at(i);
            QCOMPARE(one[3], 7); // the session
            const std::size_t from = i == 0 ? 10 : 6;
            const std::size_t count = i == 0 ? WriteBeginData : WriteMoreData;
            joined.insert(joined.end(), one.begin() + static_cast<std::ptrdiff_t>(from), one.begin() + static_cast<std::ptrdiff_t>(from + count));
        }
        // 119 + 123 + 123 + the last 75 bytes; what follows in the last report is padding.
        QCOMPARE(joined.size(), std::size_t{119 + 123 + 123 + 123});
        joined.resize(LiveFrameSize);
        QVERIFY(std::equal(joined.begin(), joined.end(), frame.begin()));
        const auto &last = reports.back();
        QVERIFY(std::all_of(last.begin() + 6 + 75, last.end(), [](std::uint8_t b) { return b == 0; }));
    }

    void aLedIndexOutOfRangeRefusesTheWholeFrame()
    {
        const std::vector<LedColor> bad{{0x3a, 1, 2, 3}, {146, 9, 9, 9}};
        QVERIFY(!buildLiveFrame(bad));
        QVERIFY(!buildLiveFrame({}));
        const std::vector<LedColor> last{{145, 1, 2, 3}};
        QVERIFY(buildLiveFrame(last)); // 0x91 is the last LED
    }

    void aLaterEntryForTheSameLedReplacesTheEarlierOne()
    {
        const std::vector<LedColor> leds{{5, 1, 2, 3}, {5, 9, 8, 7}};
        const LiveFrame frame = frameOf(leds);
        QCOMPARE(frame[2 + 15], 9);
        QCOMPARE(frame[2 + 16], 8);
        QCOMPARE(frame[2 + 17], 7);
    }

    void aFrameIsNeverWrittenOutsideASession()
    {
        QVERIFY(!liveFrameReports(frameOf({{1, 1, 1, 1}}), 0));
    }

    // ---- allowlist ----------------------------------------------------------------------------------
    void theWholeSequenceOfTheCaptureIsAllowedInOrder()
    {
        LiveCommandGuard guard;
        for (const Report &r : {sessionStart(), report({0x01, 0x02, 0x03}), setMode(2), openLive()}) {
            QCOMPARE(guard.check(r), Refusal::None);
            guard.confirmed(r);
        }
        QVERIFY(guard.fileOpen());
        sendFrame(guard, f1Green());
        QCOMPARE(guard.bytesExpected(), std::size_t{0});
        QCOMPARE(guard.check(report({0x01, 0x12})), Refusal::None); // ping
        QCOMPARE(guard.check(unbind()), Refusal::None);
        guard.confirmed(unbind());
        QVERIFY(!guard.fileOpen());
        QCOMPARE(guard.check(setMode(1)), Refusal::None);
    }

    void theModeIsNeverAnythingButOneOrTwo()
    {
        const LiveCommandGuard guard;
        QCOMPARE(guard.check(setMode(1)), Refusal::None);
        QCOMPARE(guard.check(setMode(2)), Refusal::None);
        for (const std::uint8_t bad : std::initializer_list<std::uint8_t>{0, 3, 4, 5, 6, 0xff})
            QCOMPARE(guard.check(setMode(bad)), Refusal::ModeNotAllowed); // 3 and 5 are bootloaders, 4 is sleep
        Report high = setMode(1);
        high[8] = 1; // value 0x0101
        QCOMPARE(guard.check(high), Refusal::ModeNotAllowed);
    }

    void noPropertyOtherThanTheModeCanBeWrittenNotEvenTheStoredBrightness()
    {
        const LiveCommandGuard guard;
        QCOMPARE(guard.check(setPropertyRequest(0xea, 500)), Refusal::PropertyNotAllowed);
        QCOMPARE(guard.check(setPropertyRequest(0x01, 1)), Refusal::PropertyNotAllowed);
        QCOMPARE(guard.check(getPropertyRequest(0xea)), Refusal::None); // reading is harmless
    }

    void onlyTheLiveFileInBufferZeroCanBeOpened()
    {
        const LiveCommandGuard guard;
        QCOMPARE(guard.check(report({0x01, 0x0d, 0x00, 0x21, 0x00})), Refusal::FileNotAllowed);
        QCOMPARE(guard.check(report({0x01, 0x0d, 0x00, 0x22, 0x01})), Refusal::FileNotAllowed);
        QCOMPARE(guard.check(report({0x01, 0x0d, 0x01, 0x22, 0x00})), Refusal::BufferNotAllowed);
        QCOMPARE(guard.check(report({0x01, 0x0d, 0x00, 0x22, 0x00, 0x05})), Refusal::BadPayload);
    }

    void commandsOffTheListAreRefused()
    {
        const LiveCommandGuard guard;
        // 0x0f is the factory reset (docs/protocol/README.md); the others are file creation/deletion and the like.
        for (const std::uint8_t command : std::initializer_list<std::uint8_t>{0x00, 0x03, 0x04, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0e, 0x0f, 0x10, 0x11, 0x13, 0x1a, 0x1c, 0xff})
            QCOMPARE(guard.check(report({0x01, command})), Refusal::UnknownCommand);
    }

    void aSessionStartCarriesNoSessionAndNothingAfterItsRandomBytes()
    {
        const LiveCommandGuard guard;
        QCOMPARE(guard.check(sessionStart()), Refusal::None);
        Report withSession = sessionStart();
        withSession[3] = 1;
        QCOMPARE(guard.check(withSession), Refusal::BadPayload);
        Report trailing = sessionStart();
        trailing[11] = 1;
        QCOMPARE(guard.check(trailing), Refusal::BadPayload);
    }

    void aPingWithAPayloadIsRefusedBecauseThatIsPairing()
    {
        const LiveCommandGuard guard;
        QCOMPARE(guard.check(report({0x01, 0x12})), Refusal::None);
        QCOMPARE(guard.check(report({0x01, 0x12, 0x01})), Refusal::BadPayload);
    }

    void aReportOfTheWrongSizeOrHeaderIsRefused()
    {
        const LiveCommandGuard guard;
        const Report ok = setMode(1);
        QCOMPARE(guard.check(std::span<const std::uint8_t>(ok).first(128)), Refusal::BadLength);
        Report withId = ok;
        withId[0] = 1;
        QCOMPARE(guard.check(withId), Refusal::BadHeader);
        Report answer = ok;
        answer[2] = 0; // an answer, not a request
        QCOMPARE(guard.check(answer), Refusal::BadHeader);
    }

    void aWriteBeforeTheFileIsOpenIsRefused()
    {
        const LiveCommandGuard guard;
        for (const Report &r : f1Green())
            QCOMPARE(guard.check(r), Refusal::NotOpen);
    }

    void aWriteAfterTheUnbindIsRefused()
    {
        LiveCommandGuard guard = openGuard();
        sendFrame(guard, f1Green());
        guard.confirmed(unbind());
        QCOMPARE(guard.check(f1Green()[0]), Refusal::NotOpen);
    }

    void theFrameMustAnnounceExactlyFourHundredFortyBytes()
    {
        const LiveCommandGuard guard = openGuard();
        Report begin = f1Green()[0];
        begin[6] = 0xb9; // 441
        QCOMPARE(guard.check(begin), Refusal::BadFrameLength);
        begin[6] = 0x00;
        begin[7] = 0x00; // 0
        QCOMPARE(guard.check(begin), Refusal::BadFrameLength);
    }

    void aContinuationNeedsItsBeginAndStopsAtTheLastByte()
    {
        LiveCommandGuard guard = openGuard();
        const LiveReports frame = f1Green();
        QCOMPARE(guard.check(frame[1]), Refusal::OutOfSequence); // no 06 yet
        guard.confirmed(frame[0]);
        QCOMPARE(guard.check(frame[0]), Refusal::OutOfSequence); // a 06 inside a frame
        for (const Report &rest : std::span(frame).subspan(1))
            guard.confirmed(rest);
        QCOMPARE(guard.bytesExpected(), std::size_t{0});
        QCOMPARE(guard.check(frame[1]), Refusal::OutOfSequence); // one 07 too many
        QCOMPARE(guard.check(frame[0]), Refusal::None);          // the next frame may begin
    }

    void paddingAfterTheLastFrameByteMustBeZero()
    {
        LiveCommandGuard guard = openGuard();
        LiveReports frame = f1Green();
        guard.confirmed(frame[0]);
        guard.confirmed(frame[1]);
        guard.confirmed(frame[2]);
        QCOMPARE(guard.check(frame[3]), Refusal::None);
        frame[3][6 + 75] = 0x01; // past the 75 bytes this report may carry
        QCOMPARE(guard.check(frame[3]), Refusal::BadPayload);
    }

    void aWriteBufferOtherThanZeroIsRefused()
    {
        const LiveCommandGuard guard = openGuard();
        Report begin = f1Green()[0];
        begin[5] = 1;
        QCOMPARE(guard.check(begin), Refusal::BufferNotAllowed);
    }

    void aReportTheGuardRefusesNeverChangesItsState()
    {
        LiveCommandGuard guard;
        guard.confirmed(report({0x01, 0x0d, 0x00, 0x21, 0x00})); // refused: another file
        QVERIFY(!guard.fileOpen());
        guard.confirmed(f1Green()[0]); // refused: nothing is open
        QCOMPARE(guard.bytesExpected(), std::size_t{0});
    }

    void aResetForgetsTheOpenFileAndAHalfWrittenFrame()
    {
        LiveCommandGuard guard = openGuard();
        guard.confirmed(f1Green()[0]);
        QVERIFY(guard.bytesExpected() > 0);
        guard.reset();
        QVERIFY(!guard.fileOpen());
        QCOMPARE(guard.bytesExpected(), std::size_t{0});
    }

    // ---- pacing and freeze --------------------------------------------------------------------------
    void aBurstOfTwoIsAllowedThenTheThirdFrameInTheSameInstantIsDropped()
    {
        const auto t0 = LiveClock::now();
        RateLimiter limiter(t0);
        QVERIFY(limiter.tryTake(t0));
        QVERIFY(limiter.tryTake(t0));
        QVERIFY(!limiter.tryTake(t0));
    }

    void aCallerPacingAtExactlyFiftyMillisecondsLosesNoFrame()
    {
        const auto t0 = LiveClock::now();
        RateLimiter limiter(t0);
        int refused = 0;
        for (int i = 0; i < 200; ++i)
            refused += limiter.tryTake(t0 + i * 50ms) ? 0 : 1;
        QCOMPARE(refused, 0);
    }

    void aCallerAtSixtyFramesPerSecondIsHeldToTwentyOnAverage()
    {
        const auto t0 = LiveClock::now();
        RateLimiter limiter(t0);
        int allowed = 0;
        for (int i = 0; i < 600; ++i) // 10 s at 1/60 s steps
            allowed += limiter.tryTake(t0 + std::chrono::microseconds(i * 16667)) ? 1 : 0;
        QVERIFY2(allowed >= 195 && allowed <= 205, qPrintable(QString::number(allowed)));
    }

    void aClockThatGoesBackwardsGivesNoExtraTokens()
    {
        const auto t0 = LiveClock::now();
        RateLimiter limiter(t0);
        QVERIFY(limiter.tryTake(t0 + 1s));
        QVERIFY(limiter.tryTake(t0 + 1s));
        QVERIFY(!limiter.tryTake(t0)); // earlier than the last call
        QVERIFY(!limiter.tryTake(t0 + 1s));
    }

    void twoFailuresInARowTripTheFreezePolicy()
    {
        FreezePolicy policy;
        policy.failure();
        QVERIFY(!policy.tripped());
        policy.failure();
        QVERIFY(policy.tripped());
    }

    void aSuccessBetweenTwoFailuresResetsTheCount()
    {
        FreezePolicy policy;
        policy.failure();
        policy.success();
        policy.failure();
        QVERIFY(!policy.tripped());
    }

    void onceTrippedOnlyAReplugClearsIt()
    {
        FreezePolicy policy;
        policy.failure();
        policy.failure();
        policy.success();
        QVERIFY(policy.tripped());
        policy.reset();
        QVERIFY(!policy.tripped());
        policy.failure();
        QVERIFY(!policy.tripped());
    }
};

QTEST_GUILESS_MAIN(TestKeyboardLive)
#include "test_keyboardlive.moc"
