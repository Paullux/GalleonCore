// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

// Signals: a program asks for one key to be lit, and galleond stays the only writer. The board is bounded, write-only and pure; these tests are
// where its limits are held, because they are what keeps a crashed or misbehaving program from lighting the keyboard for ever.
#include <galleon/keyboard/KeySignals.h>

#include <QTest>

#include <cmath>

using namespace galleon::keyboard;

namespace {

constexpr Rgb Amber{255, 160, 0};
constexpr Rgb Blue{0, 0, 200};

LedColor colourOf(const std::vector<LedColor> &colours, int led)
{
    for (const LedColor &colour : colours) {
        if (colour.led == led)
            return colour;
    }
    return {0xFFFF, 0, 0, 0};
}

} // namespace

class TestKeySignals : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void aPulseStartsFullBreathesDownToAFifthAndComesBack()
    {
        QCOMPARE(pulseShare(0.0), 1.0);
        QVERIFY(std::fabs(pulseShare(PulsePeriodSeconds / 2) - PulseLowestShare) < 1e-9);
        QVERIFY(std::fabs(pulseShare(PulsePeriodSeconds) - 1.0) < 1e-9);
        QVERIFY(std::fabs(pulseShare(7 * PulsePeriodSeconds) - 1.0) < 1e-9);
        for (int step = 0; step < 770; ++step) {
            const double t = step * 0.013;
            QVERIFY(pulseShare(t) >= PulseLowestShare - 1e-12);
            QVERIFY(pulseShare(t) <= 1.0 + 1e-12);
        }
    }

    void aNegativeOrNotANumberTimeShowsTheFullColour()
    {
        QCOMPARE(pulseShare(-1.0), 1.0);
        QCOMPARE(pulseShare(std::nan("")), 1.0);
    }

    void aClientNameIsShortAndPrintable_data()
    {
        QTest::addColumn<QString>("name");
        QTest::addColumn<bool>("valid");
        QTest::newRow("a helper") << "deskhelper" << true;
        QTest::newRow("with spaces and punctuation") << "calendar: next meeting" << true;
        QTest::newRow("one letter") << "a" << true;
        QTest::newRow("empty") << "" << false;
        QTest::newRow("a control character") << QString("bad\x01name") << false;
        QTest::newRow("a newline") << QString("two\nlines") << false;
        QTest::newRow("a byte beyond ASCII") << QString::fromUtf8("caf\xc3\xa9") << false;
        QTest::newRow("DEL") << QString("x\x7f") << false;
        QTest::newRow("exactly 64 bytes") << QString(64, 'a') << true;
        QTest::newRow("65 bytes") << QString(65, 'a') << false;
    }
    void aClientNameIsShortAndPrintable()
    {
        QFETCH(const QString, name);
        QFETCH(const bool, valid);
        QCOMPARE(isValidClientName(name.toStdString()), valid);
    }

    void aSteadySignalShowsItsColourUntilItEndsAndThenNothing()
    {
        SignalBoard board;
        QCOMPARE(board.set("helper", 60, Amber, SignalEffect::Steady, 100.0, 30.0), SignalBoard::Result::Ok);
        const auto during = board.overlay(110.0);
        QCOMPARE(during.size(), std::size_t{1});
        QCOMPARE(colourOf(during, 60).red, std::uint8_t{255});
        QCOMPARE(colourOf(during, 60).green, std::uint8_t{160});
        QVERIFY(!board.overlay(129.999).empty());
        QVERIFY(board.overlay(130.0).empty()); // the end is the end
        QVERIFY(board.overlay(500.0).empty());
    }

    void thePulseOfASignalStartsFullWhenItWasAskedForAndBreathesDown()
    {
        SignalBoard board;
        QCOMPARE(board.set("helper", 60, Amber, SignalEffect::Pulse, 100.0, 60.0), SignalBoard::Result::Ok);
        QCOMPARE(colourOf(board.overlay(100.0), 60).red, std::uint8_t{255});
        const LedColor low = colourOf(board.overlay(100.0 + PulsePeriodSeconds / 2), 60);
        QCOMPARE(low.red, std::uint8_t{51}); // a fifth of 255
        QCOMPARE(low.green, std::uint8_t{32});
        QCOMPARE(low.blue, std::uint8_t{0});
    }

    void aTimeOutsideTheLimitsIsRefusedAndNothingChanges()
    {
        SignalBoard board;
        const SignalBoard before = board;
        for (const double seconds : {0.0, 0.5, -3.0, MaxSignalSeconds + 1, std::nan(""), HUGE_VAL}) {
            QCOMPARE(board.set("helper", 60, Amber, SignalEffect::Steady, 0.0, seconds), SignalBoard::Result::BadSeconds);
        }
        QVERIFY(board == before);
        QCOMPARE(board.set("helper", 60, Amber, SignalEffect::Steady, 0.0, MinSignalSeconds), SignalBoard::Result::Ok);
        QCOMPARE(board.set("helper", 61, Amber, SignalEffect::Steady, 0.0, MaxSignalSeconds), SignalBoard::Result::Ok);
        QCOMPARE(board.set("helper", 62, Amber, SignalEffect::Steady, std::nan(""), 5.0), SignalBoard::Result::BadSeconds);
    }

    void aLedBeyondTheFrameAndABadNameAreRefused()
    {
        SignalBoard board;
        QCOMPARE(board.set("helper", 146, Amber, SignalEffect::Steady, 0.0, 5.0), SignalBoard::Result::BadLed);
        QCOMPARE(board.set("helper", 0xFFFF, Amber, SignalEffect::Steady, 0.0, 5.0), SignalBoard::Result::BadLed);
        QCOMPARE(board.set("", 5, Amber, SignalEffect::Steady, 0.0, 5.0), SignalBoard::Result::BadClient);
        QCOMPARE(board.set("bad\x01", 5, Amber, SignalEffect::Steady, 0.0, 5.0), SignalBoard::Result::BadClient);
        QVERIFY(board.empty());
        QCOMPARE(board.set("helper", 145, Amber, SignalEffect::Steady, 0.0, 5.0), SignalBoard::Result::Ok); // the last LED of the frame
    }

    void askingAgainForTheSameKeyReplacesTheSignalAndRestartsItsTimeAndPulse()
    {
        SignalBoard board;
        QCOMPARE(board.set("helper", 60, Amber, SignalEffect::Pulse, 0.0, 10.0), SignalBoard::Result::Ok);
        QCOMPARE(board.set("helper", 60, Blue, SignalEffect::Steady, 8.0, 10.0), SignalBoard::Result::Ok);
        QCOMPARE(board.size(), std::size_t{1});
        const auto later = board.overlay(15.0); // the first would have ended at 10
        QCOMPARE(later.size(), std::size_t{1});
        QCOMPARE(colourOf(later, 60).blue, std::uint8_t{200});
        QVERIFY(board.overlay(18.0).empty());
    }

    void whenTwoProgramsAskForTheSameKeyTheOneAskedLastIsShownAndTheOtherComesBack()
    {
        SignalBoard board;
        QCOMPARE(board.set("first", 60, Amber, SignalEffect::Steady, 0.0, 100.0), SignalBoard::Result::Ok);
        QCOMPARE(board.set("second", 60, Blue, SignalEffect::Steady, 1.0, 100.0), SignalBoard::Result::Ok);
        QCOMPARE(board.overlay(2.0).size(), std::size_t{1});
        QCOMPARE(colourOf(board.overlay(2.0), 60).blue, std::uint8_t{200});
        QVERIFY(board.clear("second"));
        QCOMPARE(colourOf(board.overlay(2.0), 60).red, std::uint8_t{255}); // the first one was only hidden
    }

    void theOverlayHasOneEntryPerLedByIncreasingLed()
    {
        SignalBoard board;
        QCOMPARE(board.set("b", 90, Amber, SignalEffect::Steady, 0.0, 9.0), SignalBoard::Result::Ok);
        QCOMPARE(board.set("a", 5, Amber, SignalEffect::Steady, 0.0, 9.0), SignalBoard::Result::Ok);
        QCOMPARE(board.set("c", 5, Blue, SignalEffect::Steady, 1.0, 9.0), SignalBoard::Result::Ok);
        const auto overlay = board.overlay(2.0);
        QCOMPARE(overlay.size(), std::size_t{2});
        QCOMPARE(overlay.at(0).led, std::uint16_t{5});
        QCOMPARE(overlay.at(1).led, std::uint16_t{90});
    }

    void aNinthProgramIsRefusedAndOneThatEndedMakesRoom()
    {
        SignalBoard board;
        for (int client = 0; client < MaxSignalClients; ++client)
            QCOMPARE(board.set("client" + std::to_string(client), static_cast<std::uint16_t>(client), Amber, SignalEffect::Steady, 0.0, 10.0), SignalBoard::Result::Ok);
        const SignalBoard before = board;
        QCOMPARE(board.set("ninth", 100, Amber, SignalEffect::Steady, 1.0, 10.0), SignalBoard::Result::TooManyClients);
        QVERIFY(board == before); // nothing changed, not even a signal dropped
        QCOMPARE(board.set("client0", 99, Blue, SignalEffect::Steady, 1.0, 10.0), SignalBoard::Result::Ok); // a program that is there may add another
        QCOMPARE(board.set("ninth", 100, Amber, SignalEffect::Steady, 11.0, 10.0), SignalBoard::Result::Ok); // after the others ended, there is room
    }

    void aProgramHasAtMostSixteenSignalsButMayReplaceOne()
    {
        SignalBoard board;
        for (int led = 0; led < MaxSignalsPerClient; ++led)
            QCOMPARE(board.set("helper", static_cast<std::uint16_t>(led), Amber, SignalEffect::Steady, 0.0, 10.0), SignalBoard::Result::Ok);
        QCOMPARE(board.set("helper", 100, Amber, SignalEffect::Steady, 0.0, 10.0), SignalBoard::Result::TooManySignals);
        QCOMPARE(board.set("helper", 3, Blue, SignalEffect::Steady, 0.0, 10.0), SignalBoard::Result::Ok); // a replacement is not one more
        QCOMPARE(board.size(), static_cast<std::size_t>(MaxSignalsPerClient));
    }

    void theBoardHasAtMostSixtyFourSignalsInAll()
    {
        SignalBoard board;
        // Eight programs of eight lit fill it; the limits of one program (16) and of programs (8) are not what stops the next one.
        for (int client = 0; client < MaxSignalClients; ++client) {
            for (int n = 0; n < 8; ++n)
                QCOMPARE(board.set("client" + std::to_string(client), static_cast<std::uint16_t>(client * 8 + n), Amber, SignalEffect::Steady, 0.0, 10.0), SignalBoard::Result::Ok);
        }
        QCOMPARE(board.size(), static_cast<std::size_t>(MaxSignalsTotal));
        QCOMPARE(board.set("client0", 130, Amber, SignalEffect::Steady, 0.0, 10.0), SignalBoard::Result::TooManySignals);
    }

    void signalsThatEndedDoNotCountAgainstTheLimits()
    {
        SignalBoard board;
        for (int led = 0; led < MaxSignalsPerClient; ++led)
            QCOMPARE(board.set("helper", static_cast<std::uint16_t>(led), Amber, SignalEffect::Steady, 0.0, 5.0), SignalBoard::Result::Ok);
        QCOMPARE(board.set("helper", 100, Amber, SignalEffect::Steady, 6.0, 5.0), SignalBoard::Result::Ok); // the sixteen are over
        QCOMPARE(board.size(), std::size_t{1});
    }

    void clearingTakesAwayWhatWasAskedAndSaysWhetherThereWasAnything()
    {
        SignalBoard board;
        QCOMPARE(board.set("a", 1, Amber, SignalEffect::Steady, 0.0, 9.0), SignalBoard::Result::Ok);
        QCOMPARE(board.set("a", 2, Amber, SignalEffect::Steady, 0.0, 9.0), SignalBoard::Result::Ok);
        QCOMPARE(board.set("b", 3, Amber, SignalEffect::Steady, 0.0, 9.0), SignalBoard::Result::Ok);
        QVERIFY(board.clear("a", 1));
        QVERIFY(!board.clear("a", 1));
        QVERIFY(!board.clear("nobody"));
        QVERIFY(!board.clear("b", 1));
        QVERIFY(board.clear("a"));
        QCOMPARE(board.size(), std::size_t{1});
        board.clearAll();
        QVERIFY(board.empty());
        QCOMPARE(board.clientCount(), std::size_t{0});
    }

    void expiringDropsOnlyWhatEnded()
    {
        SignalBoard board;
        QCOMPARE(board.set("a", 1, Amber, SignalEffect::Steady, 0.0, 5.0), SignalBoard::Result::Ok);
        QCOMPARE(board.set("a", 2, Amber, SignalEffect::Steady, 0.0, 50.0), SignalBoard::Result::Ok);
        board.expire(10.0);
        QCOMPARE(board.size(), std::size_t{1});
        QCOMPARE(board.overlay(10.0).front().led, std::uint16_t{2});
    }

    void aSignalIsDroppedAtTheMomentItEndsNotAfter()
    {
        SignalBoard board;
        QCOMPARE(board.set("a", 1, Amber, SignalEffect::Steady, 0.0, 5.0), SignalBoard::Result::Ok);
        board.expire(4.999);
        QCOMPARE(board.size(), std::size_t{1});
        board.expire(5.0);
        QVERIFY(board.empty());
    }

    void signalsAreDrawnOverTheFrameAndOnlyOnTheirLeds()
    {
        LiveFrame frame{};
        frame.at(0) = 0x12;
        frame.at(1) = 0x34; // the header of the frame is not touched
        for (std::size_t i = 2; i < frame.size(); ++i)
            frame.at(i) = 10;
        const std::vector<LedColor> lit{{60, 255, 160, 0}};
        const LiveFrame drawn = withSignals(frame, lit, LevelFull);
        QCOMPARE(drawn.at(0), std::uint8_t{0x12});
        QCOMPARE(drawn.at(1), std::uint8_t{0x34});
        QCOMPARE(drawn.at(2 + 3 * 60), std::uint8_t{255});
        QCOMPARE(drawn.at(2 + 3 * 60 + 1), std::uint8_t{160});
        QCOMPARE(drawn.at(2 + 3 * 60 + 2), std::uint8_t{0});
        QCOMPARE(drawn.at(2 + 3 * 59), std::uint8_t{10}); // the neighbour is as it was
        QCOMPARE(drawn.at(2 + 3 * 61), std::uint8_t{10});
        QCOMPARE(frame.at(2 + 3 * 60), std::uint8_t{10}); // and the frame given is not changed (a copy)
    }

    void theLevelDimsTheSignalsAndZeroMakesThemBlack()
    {
        const LiveFrame frame{};
        const std::vector<LedColor> lit{{60, 200, 100, 50}};
        const LiveFrame half = withSignals(frame, lit, 500);
        QCOMPARE(half.at(2 + 3 * 60), std::uint8_t{100});
        QCOMPARE(half.at(2 + 3 * 60 + 1), std::uint8_t{50});
        QCOMPARE(half.at(2 + 3 * 60 + 2), std::uint8_t{25});
        const LiveFrame dark = withSignals(frame, lit, 0); // the night
        QCOMPARE(dark.at(2 + 3 * 60), std::uint8_t{0});
        QCOMPARE(dark.at(2 + 3 * 60 + 1), std::uint8_t{0});
        const LiveFrame over = withSignals(frame, lit, 5000); // a level beyond the full one is the full one
        QCOMPARE(over.at(2 + 3 * 60), std::uint8_t{200});
        const LiveFrame under = withSignals(frame, lit, -300);
        QCOMPARE(under.at(2 + 3 * 60), std::uint8_t{0});
    }

    void aSignalOnALedBeyondTheFrameIsIgnoredNotWrittenOutside()
    {
        const LiveFrame frame{};
        const std::vector<LedColor> lit{{146, 255, 255, 255}, {0xFFFF, 255, 255, 255}};
        QVERIFY(withSignals(frame, lit, LevelFull) == frame);
    }

    void aBoardThatNeverHadASignalDrawsNothing()
    {
        LiveFrame frame{};
        frame.fill(7);
        const SignalBoard board;
        QVERIFY(withSignals(frame, board.overlay(1.0), LevelFull) == frame);
    }
};

QTEST_APPLESS_MAIN(TestKeySignals)
#include "test_keysignals.moc"
