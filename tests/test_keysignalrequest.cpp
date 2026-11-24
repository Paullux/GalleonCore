// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

// What the "Try a signal" panel asks for: the name of the key of a LED, and a request that is whole or refused.
#include "KeyMapFile.h"
#include "KeySignalRequest.h"

#include <QTest>

using namespace bosun;
using galleon::keyboard::KeyMap;
using galleon::keyboard::LedKind;

class TestKeySignalRequest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void aLedThatLightsAKeyGivesTheNameOfThatKey()
    {
        KeyMap map;
        QVERIFY(!map.assign({58, LedKind::Key, 0x3A, 0}).has_value()); // F1
        QCOMPARE(keyNameOfLed(map, 58), QStringLiteral("F1"));
    }

    void aLedThatIsNotAKeyOrNotInTheMapOrOutOfTheFrameGivesNoName()
    {
        KeyMap map;
        QVERIFY(!map.assign({139, LedKind::Strip, 0, 1}).has_value());
        QVERIFY(!map.assign({7, LedKind::Unlit, 0, 0}).has_value());
        QVERIFY(keyNameOfLed(map, 139).isEmpty()); // a section of the strip
        QVERIFY(keyNameOfLed(map, 7).isEmpty());   // lights nothing that was seen
        QVERIFY(keyNameOfLed(map, 100).isEmpty()); // not measured
        QVERIFY(keyNameOfLed(map, -1).isEmpty());
        QVERIFY(keyNameOfLed(map, 146).isEmpty());
        QVERIFY(keyNameOfLed(map, 100000).isEmpty());
    }

    void theMeasuredMapOfGalleondNamesTheKeysOfTheTopRow()
    {
        const auto &map = galleond::builtInKeyMap();
        QCOMPARE(keyNameOfLed(map, 58), QStringLiteral("F1"));
        QCOMPARE(keyNameOfLed(map, 59), QStringLiteral("F2"));
        QCOMPARE(keyNameOfLed(map, 60), QStringLiteral("F3"));
    }

    void aWholeRequestKeepsEverythingAsAsked()
    {
        QString why;
        const auto request = makeKeySignalRequest(QStringLiteral("F3"), QStringLiteral("#ff7800"), true, 600, &why);
        QVERIFY2(request.has_value(), qPrintable(why));
        const KeySignalRequest expected{QStringLiteral("F3"), 255, 120, 0, true, 600};
        QCOMPARE(request.value_or(KeySignalRequest{}), expected);
        const auto steady = makeKeySignalRequest(QStringLiteral("Left Control"), QStringLiteral("#00AaFF"), false, 1);
        QCOMPARE(steady.value_or(KeySignalRequest{}), (KeySignalRequest{QStringLiteral("Left Control"), 0, 170, 255, false, 1}));
        QVERIFY(makeKeySignalRequest(QStringLiteral("F3"), QStringLiteral("#ffffff"), false, LongestSignalSeconds).has_value());
    }

    void aRequestThatIsNotWholeIsRefusedWithAReason_data()
    {
        QTest::addColumn<QString>("key");
        QTest::addColumn<QString>("colour");
        QTest::addColumn<int>("seconds");
        QTest::newRow("no key") << "" << "#ffffff" << 60;
        QTest::newRow("a key name that is too long") << QString(33, QLatin1Char('a')) << "#ffffff" << 60;
        QTest::newRow("a colour without the hash") << "F3" << "ffffff" << 60;
        QTest::newRow("a colour too short") << "F3" << "#fff" << 60;
        QTest::newRow("a colour with a letter that is not a digit") << "F3" << "#gg0000" << 60;
        QTest::newRow("a colour with something after it") << "F3" << "#ffffff00" << 60;
        QTest::newRow("a colour name") << "F3" << "red" << 60;
        QTest::newRow("no time") << "F3" << "#ffffff" << 0;
        QTest::newRow("a negative time") << "F3" << "#ffffff" << -5;
        QTest::newRow("longer than a day") << "F3" << "#ffffff" << LongestSignalSeconds + 1;
    }
    void aRequestThatIsNotWholeIsRefusedWithAReason()
    {
        QFETCH(const QString, key);
        QFETCH(const QString, colour);
        QFETCH(const int, seconds);
        QString why;
        QVERIFY(!makeKeySignalRequest(key, colour, false, seconds, &why).has_value());
        QVERIFY(!why.isEmpty());
    }
};

QTEST_APPLESS_MAIN(TestKeySignalRequest)
#include "test_keysignalrequest.moc"
