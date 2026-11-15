// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: LGPL-3.0-or-later

// The order of questions of the key calibration. Pure: nothing is lit, read or saved.

#include <galleon/keyboard/CalibrationFlow.h>

#include <QTest>

using namespace galleon::keyboard;

class CalibrationFlowTest : public QObject
{
    Q_OBJECT

private slots:
    void itStartsWithLedZeroAndNothingPassed()
    {
        const CalibrationFlow flow;
        QCOMPARE(flow.currentLed(), std::optional<std::uint16_t>(0));
        QCOMPARE(flow.passed(), std::size_t{0});
        QVERIFY(!flow.finished());
    }

    void aKeyAnswerIsRecordedAndTheNextLedIsAsked()
    {
        CalibrationFlow flow;
        QCOMPARE(flow.answerKey(0x04), AnswerOutcome::Recorded); // A
        QCOMPARE(flow.currentLed(), std::optional<std::uint16_t>(1));
        QCOMPARE(flow.map().ledForKey(0x04), std::optional<std::uint16_t>(0));
    }

    void aKeyThatAnotherLedAlreadyLightsIsNotRecordedAndTheSameLedIsAskedAgain()
    {
        CalibrationFlow flow;
        QCOMPARE(flow.answerKey(0x04), AnswerOutcome::Recorded);
        QCOMPARE(flow.answerKey(0x04), AnswerOutcome::KeyAlreadyUsed); // LED 1: the person pressed A again
        QCOMPARE(flow.conflictingLed(), std::optional<std::uint16_t>(0));
        QCOMPARE(flow.currentLed(), std::optional<std::uint16_t>(1));  // still LED 1
        QVERIFY(!flow.map().assignmentFor(1).has_value());
        QCOMPARE(flow.answerKey(0x05), AnswerOutcome::Recorded);       // the right key is accepted
        QVERIFY(!flow.conflictingLed().has_value());
    }

    void anUnknownKeyOrABadStripPositionIsRefusedAndNothingMoves()
    {
        CalibrationFlow flow;
        QCOMPARE(flow.answerKey(0x03), AnswerOutcome::Refused);
        QCOMPARE(flow.answerStrip(0), AnswerOutcome::Refused);
        QCOMPARE(flow.answerStrip(65), AnswerOutcome::Refused);
        QCOMPARE(flow.currentLed(), std::optional<std::uint16_t>(0));
        QCOMPARE(flow.map().assignedCount(), std::size_t{0});
    }

    void stripAndUnlitAnswersAreRecorded()
    {
        CalibrationFlow flow;
        QCOMPARE(flow.answerUnlit(), AnswerOutcome::Recorded);
        QCOMPARE(flow.answerStrip(3), AnswerOutcome::Recorded);
        const auto first = flow.map().assignmentFor(0).value_or(LedAssignment{});
        const auto second = flow.map().assignmentFor(1).value_or(LedAssignment{});
        QCOMPARE(first.kind, LedKind::Unlit);
        QCOMPARE(second.kind, LedKind::Strip);
        QCOMPARE(second.stripPosition, std::uint8_t{3});
    }

    void aSkippedLedHasNoAnswerAndIsListed()
    {
        CalibrationFlow flow;
        flow.skip();
        QCOMPARE(flow.answerUnlit(), AnswerOutcome::Recorded);
        QVERIFY(!flow.map().assignmentFor(0).has_value());
        QCOMPARE(flow.skippedLeds(), (std::vector<std::uint16_t>{0}));
        QCOMPARE(flow.passed(), std::size_t{2});
    }

    void backForgetsTheLastAnswerAndAsksTheLedAgain()
    {
        CalibrationFlow flow;
        QCOMPARE(flow.answerKey(0x04), AnswerOutcome::Recorded);
        QCOMPARE(flow.answerKey(0x05), AnswerOutcome::Recorded);
        QCOMPARE(flow.back(), AnswerOutcome::Recorded);
        QCOMPARE(flow.currentLed(), std::optional<std::uint16_t>(1));
        QVERIFY(!flow.map().assignmentFor(1).has_value());
        QVERIFY(!flow.map().ledForKey(0x05).has_value()); // B is free again
        QCOMPARE(flow.map().ledForKey(0x04), std::optional<std::uint16_t>(0));
        QCOMPARE(flow.back(), AnswerOutcome::Recorded);
        QCOMPARE(flow.back(), AnswerOutcome::NothingToUndo); // at the first question
        QCOMPARE(flow.currentLed(), std::optional<std::uint16_t>(0));
    }

    void backAlsoUndoesASkip()
    {
        CalibrationFlow flow;
        flow.skip();
        QCOMPARE(flow.back(), AnswerOutcome::Recorded);
        QCOMPARE(flow.currentLed(), std::optional<std::uint16_t>(0));
        QVERIFY(flow.skippedLeds().empty());
    }

    void ledsThatAreAlreadyKnownAreNotAskedAgain()
    {
        KeyMap known;
        QVERIFY(!known.assign(LedAssignment{.led = 0, .kind = LedKind::Unlit}).has_value());
        QVERIFY(!known.assign(LedAssignment{.led = 1, .kind = LedKind::Key, .key = 0x04}).has_value());
        CalibrationFlow flow(known);
        QCOMPARE(flow.currentLed(), std::optional<std::uint16_t>(2));
        QCOMPARE(flow.answerKey(0x05), AnswerOutcome::Recorded);
        QCOMPARE(flow.map().assignedCount(), std::size_t{3});
        // The key of a known answer is taken: pressing A for LED 3 is a conflict.
        QCOMPARE(flow.answerKey(0x04), AnswerOutcome::KeyAlreadyUsed);
        QCOMPARE(flow.conflictingLed(), std::optional<std::uint16_t>(1));
    }

    void itFinishesAfterTheLastLedAndRefusesFurtherAnswers()
    {
        CalibrationFlow flow;
        for (std::size_t i = 0; i < LiveLedCount; ++i) {
            QVERIFY(!flow.finished());
            QCOMPARE(flow.answerUnlit(), AnswerOutcome::Recorded);
        }
        QVERIFY(flow.finished());
        QVERIFY(!flow.currentLed().has_value());
        QCOMPARE(flow.answerKey(0x04), AnswerOutcome::Refused);
        QCOMPARE(flow.answerUnlit(), AnswerOutcome::Refused);
        flow.skip(); // nothing to skip, nothing breaks
        QCOMPARE(flow.map().assignedCount(), LiveLedCount);
        QCOMPARE(flow.back(), AnswerOutcome::Recorded); // the last answer can still be undone
        QVERIFY(!flow.finished());
        QCOMPARE(flow.currentLed(), std::optional<std::uint16_t>(145));
    }
};

QTEST_APPLESS_MAIN(CalibrationFlowTest)
#include "test_calibrationflow.moc"
