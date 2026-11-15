// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: LGPL-3.0-or-later

// The identity of a physical key (its USB HID usage) and the conversions from the codes of Linux (evdev) and Windows
// (scan codes). Pure tables: no keyboard is read, nothing is sent.

#include <galleon/keyboard/KeyIdentity.h>

#include <QTest>

#include <cstring>
#include <set>
#include <utility>

using namespace galleon::keyboard;

class KeyIdentityTest : public QObject
{
    Q_OBJECT

private slots:
    void aKeyKeepsItsIdentityOnEitherSystem_data()
    {
        QTest::addColumn<unsigned>("evdev");
        QTest::addColumn<unsigned>("scan");
        QTest::addColumn<bool>("extended");
        QTest::addColumn<unsigned>("usage");
        // Anchors that do not come from our own table: the HID usage of A is 0x04, of F1 0x3A and of the left Windows
        // (GUI) key 0xE3 (HID Usage Tables); KEY_A is 30 and KEY_F1 59 (input-event-codes.h); the scan codes of
        // A, F1 and the left Windows key are 1E, 3B and E0 5B (Microsoft's scan code specification).
        QTest::newRow("A") << 30U << 0x1EU << false << 0x04U;
        QTest::newRow("F1") << 59U << 0x3BU << false << 0x3AU;
        QTest::newRow("Escape") << 1U << 0x01U << false << 0x29U;
        QTest::newRow("Left Windows") << 125U << 0x5BU << true << 0xE3U;
    }
    void aKeyKeepsItsIdentityOnEitherSystem()
    {
        QFETCH(const unsigned, evdev);
        QFETCH(const unsigned, scan);
        QFETCH(const bool, extended);
        QFETCH(const unsigned, usage);
        QCOMPARE(hidUsageFromEvdev(evdev), std::optional<HidUsage>(static_cast<HidUsage>(usage)));
        QCOMPARE(hidUsageFromWindowsScanCode(scan, extended), std::optional<HidUsage>(static_cast<HidUsage>(usage)));
    }

    void theExtendedFlagTellsTheTwoControlKeysApart()
    {
        // Left and right Control share the scan code 1D; only the E0 prefix differs.
        QCOMPARE(hidUsageFromWindowsScanCode(0x1D, false), std::optional<HidUsage>(0xE0));
        QCOMPARE(hidUsageFromWindowsScanCode(0x1D, true), std::optional<HidUsage>(0xE4));
        QCOMPARE(hidUsageFromWindowsScanCode(0x38, false), std::optional<HidUsage>(0xE2));
        QCOMPARE(hidUsageFromWindowsScanCode(0x38, true), std::optional<HidUsage>(0xE6));
    }

    void anUnknownCodeGivesNoIdentity()
    {
        QVERIFY(!hidUsageFromEvdev(0).has_value());
        QVERIFY(!hidUsageFromEvdev(65535).has_value());
        QVERIFY(!hidUsageFromWindowsScanCode(0, false).has_value()); // 0 is "no scan code", not a key
        QVERIFY(!hidUsageFromWindowsScanCode(0xFFFF, true).has_value());
        QVERIFY(!hidUsageFromWindowsScanCode(0x2B, true).has_value()); // the scan code of the backslash is not extended
    }

    void everyKeyHasAUniqueIdentityOnEachSystem()
    {
        // Walks every HID usage of the page and checks each code leads back to exactly one usage.
        std::set<unsigned> evdevSeen;
        int known = 0;
        for (unsigned usage = 0; usage <= 0xFF; ++usage) {
            const auto evdev = evdevFromHidUsage(static_cast<HidUsage>(usage));
            if (!evdev)
                continue;
            ++known;
            QVERIFY2(evdevSeen.insert(*evdev).second, "two keys share one evdev code");
            QCOMPARE(hidUsageFromEvdev(*evdev), std::optional<HidUsage>(static_cast<HidUsage>(usage)));
            QVERIFY(keyName(static_cast<HidUsage>(usage)).has_value());
        }
        QCOMPARE(known, 88);
        // Every Windows scan code (and its extended flag) also leads to one usage, and back.
        std::set<std::pair<unsigned, bool>> scanSeen;
        for (unsigned scan = 1; scan <= 0x7F; ++scan) {
            for (const bool extended : {false, true}) {
                const auto usage = hidUsageFromWindowsScanCode(scan, extended);
                if (!usage)
                    continue;
                QVERIFY2(scanSeen.insert({scan, extended}).second, "two keys share one scan code");
                QVERIFY(evdevFromHidUsage(*usage).has_value());
            }
        }
        // 88 keys, one of them (Pause) without a scan code.
        QCOMPARE(scanSeen.size(), std::size_t{87});
    }

    void qtNativeScanCodesNameTheSamePhysicalKeyOnBothSystems()
    {
        // Linux: the XKB keycode is the evdev code plus 8 (A = evdev 30 = keycode 38).
        QCOMPARE(hidUsageFromQtNativeScanCode(38, QtScanCodeSystem::Linux), std::optional<HidUsage>(0x04));
        // Windows: the scan code of A is 1E; the extended flag, in either form, tells the right Control (E0 1D) from the left (1D).
        QCOMPARE(hidUsageFromQtNativeScanCode(0x1E, QtScanCodeSystem::Windows), std::optional<HidUsage>(0x04));
        QCOMPARE(hidUsageFromQtNativeScanCode(0x1D, QtScanCodeSystem::Windows), std::optional<HidUsage>(0xE0));
        QCOMPARE(hidUsageFromQtNativeScanCode(0x11D, QtScanCodeSystem::Windows), std::optional<HidUsage>(0xE4));
        QCOMPARE(hidUsageFromQtNativeScanCode(0xE01D, QtScanCodeSystem::Windows), std::optional<HidUsage>(0xE4));
    }

    void anUnknownQtScanCodeGivesNoIdentity()
    {
        QVERIFY(!hidUsageFromQtNativeScanCode(0, QtScanCodeSystem::Linux).has_value());
        QVERIFY(!hidUsageFromQtNativeScanCode(8, QtScanCodeSystem::Linux).has_value()); // evdev 0 is not a key
        QVERIFY(!hidUsageFromQtNativeScanCode(4000000, QtScanCodeSystem::Linux).has_value());
        QVERIFY(!hidUsageFromQtNativeScanCode(0, QtScanCodeSystem::Windows).has_value());
        QVERIFY(!hidUsageFromQtNativeScanCode(0x7F00 | 0x1E, QtScanCodeSystem::Windows).has_value()); // a high byte that is no flag
        QVERIFY(!hidUsageFromQtNativeScanCode(0x1'0000, QtScanCodeSystem::Windows).has_value());
    }

    void aKeyHasAnEnglishNameForThePersonCalibrating()
    {
        QCOMPARE(QString::fromLatin1(keyName(0x04).value_or("")), QStringLiteral("A"));
        QCOMPARE(QString::fromLatin1(keyName(0x3A).value_or("")), QStringLiteral("F1"));
        QCOMPARE(QString::fromLatin1(keyName(0xE4).value_or("")), QStringLiteral("Right Control"));
        QVERIFY(!keyName(0x03).has_value());
    }
};

QTEST_APPLESS_MAIN(KeyIdentityTest)
#include "test_keyidentity.moc"
