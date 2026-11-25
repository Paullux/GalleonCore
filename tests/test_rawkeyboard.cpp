// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

// What the key glow decides from a Raw Input message of Windows: which key went down, and whether the message comes from the Galleon keyboard. Both are pure, so they are
// tested on every system, with the codes of Microsoft's scan code table (the ones of KeyIdentity.h) and the form of the names of HID devices.
#include "RawKeyboard.h"

#include <QTest>

using namespace galleond;

class TestRawKeyboard : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void theFlagsHaveTheValuesOfWinuserH()
    {
        QCOMPARE(RawKeyBreak, 1U);
        QCOMPARE(RawKeyE0, 2U);
        QCOMPARE(RawKeyE1, 4U);
    }

    void aKeyThatWentDownGivesItsUsage_data()
    {
        QTest::addColumn<unsigned>("makeCode");
        QTest::addColumn<unsigned>("flags");
        QTest::addColumn<unsigned>("usage");
        QTest::newRow("F2") << 0x3CU << 0U << 0x3BU;
        QTest::newRow("F3") << 0x3DU << 0U << 0x3CU;
        QTest::newRow("A (the place of Q on an AZERTY keyboard)") << 0x1EU << 0U << 0x04U;
        QTest::newRow("left Control") << 0x1DU << 0U << 0xE0U;
        QTest::newRow("right Control (extended)") << 0x1DU << RawKeyE0 << 0xE4U;
        QTest::newRow("the Delete key (extended)") << 0x53U << RawKeyE0 << 0x4CU;
    }
    void aKeyThatWentDownGivesItsUsage()
    {
        QFETCH(const unsigned, makeCode);
        QFETCH(const unsigned, flags);
        QFETCH(const unsigned, usage);
        const auto found = usageOfRawKeyDown({makeCode, flags});
        QVERIFY(found.has_value());
        QCOMPARE(static_cast<unsigned>(found.value_or(0)), usage);
    }

    void aKeyThatWentUpGivesNothing()
    {
        QVERIFY(!usageOfRawKeyDown({0x3C, RawKeyBreak}).has_value());
        QVERIFY(!usageOfRawKeyDown({0x1D, RawKeyBreak | RawKeyE0}).has_value());
    }

    void theSequenceOfThePauseKeyGivesNothing()
    {
        QVERIFY(!usageOfRawKeyDown({0x1D, RawKeyE1}).has_value());
        QVERIFY(!usageOfRawKeyDown({0x45, RawKeyE1}).has_value());
    }

    void aScanCodeThatIsNotInTheTableGivesNothing()
    {
        QVERIFY(!usageOfRawKeyDown({0, 0}).has_value());       // "no scan code"
        QVERIFY(!usageOfRawKeyDown({0xFF, 0}).has_value());    // the overrun that Windows reports
        QVERIFY(!usageOfRawKeyDown({0xFFFF, 0}).has_value());
        QVERIFY(!usageOfRawKeyDown({0x3C, RawKeyE0}).has_value()); // F2 is not an extended key
    }

    void theExtendedFlagMakesAnotherKeyOfTheSameCode()
    {
        const auto plain = usageOfRawKeyDown({0x1D, 0});
        const auto extended = usageOfRawKeyDown({0x1D, RawKeyE0});
        QVERIFY(plain.has_value() && extended.has_value());
        QVERIFY(plain.value_or(0) != extended.value_or(0));
    }

    void onlyTheGalleonKeyboardIsAccepted_data()
    {
        QTest::addColumn<QString>("name");
        QTest::addColumn<bool>("accepted");
        QTest::newRow("the keyboard interface") << QStringLiteral("\\\\?\\HID#VID_1B1C&PID_2B0C&MI_00#8&2f9b1c3a&0&0000#{884b96c3-56ef-11d1-bc8c-00a0c91405dd}") << true;
        QTest::newRow("a collection of it") << QStringLiteral("\\\\?\\HID#VID_1B1C&PID_2B0C&MI_00&Col01#9&1c2d3e4f&0&0000#{884b96c3-56ef-11d1-bc8c-00a0c91405dd}") << true;
        QTest::newRow("in lower case") << QStringLiteral("\\\\?\\hid#vid_1b1c&pid_2b0c&mi_00#8&2f9b&0&0000#{884b96c3}") << true;
        QTest::newRow("the Stream Deck module of the same product") << QStringLiteral("\\\\?\\HID#VID_1B1C&PID_2B18&MI_01#7&1a2b3c4d&0&0000#{884b96c3-56ef-11d1-bc8c-00a0c91405dd}") << false;
        QTest::newRow("another Corsair product") << QStringLiteral("\\\\?\\HID#VID_1B1C&PID_1B2D&MI_00#8&2f9b&0&0000#{884b96c3}") << false;
        QTest::newRow("another keyboard") << QStringLiteral("\\\\?\\HID#VID_046D&PID_C31C#7&3a1b2c3d&0&0000#{884b96c3-56ef-11d1-bc8c-00a0c91405dd}") << false;
        QTest::newRow("the same numbers in another place") << QStringLiteral("\\\\?\\ACPI#PNP0303#4&1b1c&2b0c#{884b96c3}") << false;
        QTest::newRow("a product number that goes on") << QStringLiteral("\\\\?\\HID#VID_1B1C&PID_2B0C1&MI_00#8&2f9b&0&0000#{884b96c3}") << false;
        QTest::newRow("a vendor number that goes on") << QStringLiteral("\\\\?\\HID#VID_1B1C1&PID_2B0C&MI_00#8&2f9b&0&0000#{884b96c3}") << false;
        QTest::newRow("empty") << QString() << false;
        QTest::newRow("a name far longer than any real one") << (QStringLiteral("VID_1B1C&PID_2B0C") + QString(2000, QLatin1Char('x'))) << false;
    }
    void onlyTheGalleonKeyboardIsAccepted()
    {
        QFETCH(const QString, name);
        QFETCH(const bool, accepted);
        QCOMPARE(isGalleonKeyboardDeviceName(name), accepted);
    }
};

QTEST_APPLESS_MAIN(TestRawKeyboard)
#include "test_rawkeyboard.moc"
