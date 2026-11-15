// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: LGPL-3.0-or-later

// The map between the LEDs of the live frame and the physical keys: validation of what a calibration may record.
// Pure model, nothing is sent and no key is read.

#include <galleon/keyboard/KeyMap.h>

#include <QTest>

using namespace galleon::keyboard;

namespace {

LedAssignment key(std::uint16_t led, HidUsage usage)
{
    LedAssignment a;
    a.led = led;
    a.kind = LedKind::Key;
    a.key = usage;
    return a;
}

LedAssignment strip(std::uint16_t led, std::uint8_t position)
{
    LedAssignment a;
    a.led = led;
    a.kind = LedKind::Strip;
    a.stripPosition = position;
    return a;
}

LedAssignment unlit(std::uint16_t led)
{
    LedAssignment a;
    a.led = led;
    a.kind = LedKind::Unlit;
    return a;
}

} // namespace

class KeyMapTest : public QObject
{
    Q_OBJECT

private slots:
    void aNewMapKnowsNothingAndEveryLedIsMissing()
    {
        const KeyMap map;
        QCOMPARE(map.assignedCount(), std::size_t{0});
        QCOMPARE(map.unassignedLeds().size(), LiveLedCount);
        QVERIFY(!map.assignmentFor(0x3A).has_value());
        QVERIFY(!map.ledForKey(0x3A).has_value());
    }

    void aConfirmedKeyIsFoundFromItsLedAndFromItself()
    {
        KeyMap map;
        QVERIFY(!map.assign(key(0x3A, 0x3A)).has_value()); // F1
        QCOMPARE(map.assignmentFor(0x3A), std::optional<LedAssignment>(key(0x3A, 0x3A)));
        QCOMPARE(map.ledForKey(0x3A), std::optional<std::uint16_t>(0x3A));
        QCOMPARE(map.assignedCount(), std::size_t{1});
    }

    void aKeyOutsideTheLedIndexRangeCanStillBeMapped()
    {
        // The modifiers have usages 0xE0 to 0xE7, beyond the 146 LEDs: their LED is somewhere else, found by calibration.
        KeyMap map;
        QVERIFY(!map.assign(key(0x87, 0xE1)).has_value()); // left Shift lit by LED 0x87, say
        QCOMPARE(map.ledForKey(0xE1), std::optional<std::uint16_t>(0x87));
    }

    void aLedBeyondTheLiveFrameIsRefused()
    {
        KeyMap map;
        QCOMPARE(map.assign(key(146, 0x04)), std::optional<AssignError>(AssignError::LedOutOfRange));
        QCOMPARE(map.assign(unlit(65535)), std::optional<AssignError>(AssignError::LedOutOfRange));
        QCOMPARE(map.assignedCount(), std::size_t{0});
        QVERIFY(!map.assignmentFor(146).has_value());
    }

    void aKeyNotInTheTableIsRefused()
    {
        KeyMap map;
        QCOMPARE(map.assign(key(5, 0x03)), std::optional<AssignError>(AssignError::UnknownKey)); // 0x03 is not a key
        QCOMPARE(map.assignedCount(), std::size_t{0});
    }

    void oneKeyIsNotLitByTwoLeds()
    {
        KeyMap map;
        QVERIFY(!map.assign(key(0x04, 0x04)).has_value());
        QCOMPARE(map.assign(key(0x05, 0x04)), std::optional<AssignError>(AssignError::KeyAlreadyUsed));
        QCOMPARE(map.ledForKey(0x04), std::optional<std::uint16_t>(0x04)); // the first answer stands
        QVERIFY(!map.assignmentFor(0x05).has_value());
    }

    void correctingTheAnswerOfALedReplacesIt()
    {
        KeyMap map;
        QVERIFY(!map.assign(key(0x04, 0x04)).has_value());
        QVERIFY(!map.assign(key(0x04, 0x05)).has_value()); // the person corrects: this LED is B
        QCOMPARE(map.assignmentFor(0x04), std::optional<LedAssignment>(key(0x04, 0x05)));
        QVERIFY(!map.ledForKey(0x04).has_value());          // A is free again
        QVERIFY(!map.assign(key(0x04, 0x05)).has_value());   // the same answer twice is not a conflict
        QCOMPARE(map.assignedCount(), std::size_t{1});
    }

    void aLedChangedFromAKeyToUnlitFreesTheKey()
    {
        KeyMap map;
        QVERIFY(!map.assign(key(0x04, 0x04)).has_value());
        QVERIFY(!map.assign(unlit(0x04)).has_value());
        QVERIFY(!map.ledForKey(0x04).has_value());
        QVERIFY(!map.assign(key(0x05, 0x04)).has_value());
    }

    void stripSectionsAreCountedFromOneUpToABound()
    {
        KeyMap map;
        QVERIFY(!map.assign(strip(0x82, 1)).has_value());
        QVERIFY(!map.assign(strip(0x83, MaxStripPosition)).has_value());
        QCOMPARE(map.assign(strip(0x84, 0)), std::optional<AssignError>(AssignError::BadStripPosition));
        QCOMPARE(map.assign(strip(0x84, MaxStripPosition + 1)), std::optional<AssignError>(AssignError::BadStripPosition));
        QVERIFY(!map.assignmentFor(0x84).has_value());
    }

    void anUnlitLedIsAnAnswerToo()
    {
        // The 18 indices that lit nothing are recorded as such, so they are no longer "missing".
        KeyMap map;
        QVERIFY(!map.assign(unlit(0x00)).has_value());
        QCOMPARE(map.assignedCount(), std::size_t{1});
        QCOMPARE(map.unassignedLeds().size(), LiveLedCount - 1);
        QCOMPARE(map.assignmentFor(0x00).value_or(key(0, 0x04)).kind, LedKind::Unlit);
    }

    void theAnswersAreListedByIncreasingLed()
    {
        KeyMap map;
        QVERIFY(!map.assign(strip(0x90, 2)).has_value());
        QVERIFY(!map.assign(key(0x3A, 0x3A)).has_value());
        QVERIFY(!map.assign(unlit(0x00)).has_value());
        const std::vector<LedAssignment> expected{unlit(0x00), key(0x3A, 0x3A), strip(0x90, 2)};
        QVERIFY(map.assignments() == expected);
    }

    void theDocumentedHypothesisIsOnlyAHint()
    {
        QCOMPARE(assumedLedForKey(0x3A), std::optional<std::uint16_t>(0x3A)); // F1
        QVERIFY(!assumedLedForKey(0xE0).has_value());  // modifiers are beyond the 146 LEDs
        QVERIFY(!assumedLedForKey(0x03).has_value());  // not a key
        const KeyMap map;
        QVERIFY(!map.ledForKey(0x3A).has_value());     // asking for a hint records nothing
    }
};

QTEST_APPLESS_MAIN(KeyMapTest)
#include "test_keymap.moc"
