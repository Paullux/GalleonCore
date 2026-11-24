// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

// The keys down in an input report of the keyboard, from the layout its own report descriptor declares (docs/protocol/README.md, "Keyboard input
// reports"). The vectors are written out byte by byte from that layout, not built with the code's own arithmetic, so that a wrong offset is caught.
// No report captured while typing is kept anywhere: what the keyboard sent in the measurement of 2026-11-24 is only its shape (ID 1, 18 bytes).
#include <galleon/keyboard/KeyReport.h>

#include <QTest>

#include <algorithm>
#include <array>
#include <vector>

using namespace galleon::keyboard;

namespace {

using Report = std::array<std::uint8_t, KeyReportSize>;
using Usages = std::vector<HidUsage>;

// A report with nothing down: the ID and 17 zero bytes.
Report quiet()
{
    Report report{};
    report[0] = KeyReportId;
    return report;
}

Usages down(const Report &report)
{
    const auto usages = keysDown(report);
    return usages ? *usages : Usages{0xFFFF}; // 0xFFFF stands for "refused": never a usage of this report
}

} // namespace

class TestKeyReport : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void aReportWithNothingDownGivesNoKey()
    {
        QVERIFY(keysDown(quiet()).has_value());
        QVERIFY(down(quiet()).empty());
    }

    void theLetterAIsBitFourOfTheFirstByteOfTheBitmap()
    {
        Report report = quiet();
        report[2] = 0x10; // usage 4
        QCOMPARE(down(report), (Usages{0x04}));
    }

    void f3IsBitFourOfTheEighthByteOfTheBitmap()
    {
        Report report = quiet();
        report[9] = 0x10; // usage 0x3C = 60: byte 2 + 7, bit 4
        QCOMPARE(down(report), (Usages{0x3C}));
    }

    void theLastUsageOfTheBitmapIsBitThreeOfTheLastByte()
    {
        Report report = quiet();
        report[16] = 0x08; // usage 0x73 = 115: byte 2 + 14, bit 3
        QCOMPARE(down(report), (Usages{0x73}));
    }

    void theFourBitsBeyondTheLastUsageAreNeverAKey()
    {
        Report report = quiet();
        report[16] = 0xF0; // bits 116 to 119
        QVERIFY(down(report).empty());
    }

    void theModifiersAreTheUsagesE0ToE7()
    {
        Report left = quiet();
        left[1] = 0x01;
        QCOMPARE(down(left), (Usages{0xE0})); // left Control
        Report right = quiet();
        right[1] = 0x80;
        QCOMPARE(down(right), (Usages{0xE7})); // right GUI
        Report shift = quiet();
        shift[1] = 0x02;
        QCOMPARE(down(shift), (Usages{0xE1}));
    }

    void theInternationalKeysAreTheUsages87To8E()
    {
        Report first = quiet();
        first[17] = 0x01;
        QCOMPARE(down(first), (Usages{0x87}));
        Report last = quiet();
        last[17] = 0x80;
        QCOMPARE(down(last), (Usages{0x8E}));
    }

    void noEventAndTheRolloverCodesAreNotKeys()
    {
        Report report = quiet();
        report[2] = 0x0F; // usages 0 to 3
        QVERIFY(down(report).empty());
        report[2] = 0x1F; // and usage 4, which is a key
        QCOMPARE(down(report), (Usages{0x04}));
    }

    void severalKeysComeInIncreasingOrderWithTheModifiersLast()
    {
        Report report = quiet();
        report[1] = 0x22;  // left Shift (E1) and right Shift (E5)
        report[2] = 0x30;  // usages 4 and 5
        report[9] = 0x10;  // usage 0x3C
        report[17] = 0x02; // usage 0x88
        QCOMPARE(down(report), (Usages{0x04, 0x05, 0x3C, 0x88, 0xE1, 0xE5}));
    }

    void everyBitSetGivesEachKeyOnceAndNeverMoreThanTheMaximum()
    {
        Report report;
        report.fill(0xFF);
        report[0] = KeyReportId;
        const Usages usages = down(report);
        QCOMPARE(usages.size(), MaxKeysDown);
        QVERIFY(std::ranges::is_sorted(usages));
        QCOMPARE(std::ranges::adjacent_find(usages), usages.end()); // no usage twice
        QCOMPARE(usages.front(), HidUsage{0x04});
        QCOMPARE(usages.back(), HidUsage{0xE7});
    }

    void aReportThatIsNotEighteenBytesIsRefused()
    {
        for (const std::size_t size : {0U, 1U, 2U, 17U, 19U, 64U}) {
            std::vector<std::uint8_t> bytes(size, 0);
            if (size > 0)
                bytes[0] = KeyReportId;
            QVERIFY2(!keysDown(bytes).has_value(), qPrintable(QString::number(size)));
        }
    }

    void aReportWithAnotherIdIsRefusedEvenWithTheRightLength()
    {
        for (const std::uint8_t id : std::array<std::uint8_t, 6>{0, 2, 3, 8, 0x80, 0xFF}) {
            Report report = quiet();
            report[0] = id;
            report[2] = 0x10;
            QVERIFY2(!keysDown(report).has_value(), qPrintable(QString::number(id)));
        }
    }

    void theMediaKeyReportIsRecognisedByItsIdAndItsLengthOnly()
    {
        QVERIFY(isMediaKeyReport(std::array<std::uint8_t, 3>{2, 0xE9, 0}));
        QVERIFY(isMediaKeyReport(std::array<std::uint8_t, 3>{2, 0, 0}));
        QVERIFY(!isMediaKeyReport(std::array<std::uint8_t, 3>{1, 0, 0}));
        QVERIFY(!isMediaKeyReport(std::array<std::uint8_t, 4>{2, 0, 0, 0}));
        QVERIFY(!isMediaKeyReport(std::array<std::uint8_t, 2>{2, 0}));
        QVERIFY(!isMediaKeyReport(std::span<const std::uint8_t>{}));
        QVERIFY(!keysDown(std::array<std::uint8_t, 3>{2, 0xE9, 0}).has_value()); // and it is not a key report
    }

    void theBytesOfTheReportAreNotKeptAnywhereButInTheCallersBuffer()
    {
        // Asking twice gives the same answer from the same bytes and nothing carries over: the function has no state.
        Report report = quiet();
        report[2] = 0x10;
        QCOMPARE(down(report), down(report));
        QVERIFY(down(quiet()).empty());
    }
};

QTEST_APPLESS_MAIN(TestKeyReport)
#include "test_keyreport.moc"
