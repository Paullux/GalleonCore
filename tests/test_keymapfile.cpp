// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

// keymap.json: the strict format of the map between the LEDs of the live frame and the keys, and its atomic write. Pure
// files: no keyboard is opened, no key is read.

#include "KeyMapFile.h"

#include <QFile>
#include <QTemporaryDir>
#include <QTest>

using namespace galleond;
using galleon::keyboard::LedAssignment;
using galleon::keyboard::LedKind;

namespace {

LedAssignment keyEntry(std::uint16_t led, galleon::keyboard::HidUsage usage)
{
    LedAssignment a;
    a.led = led;
    a.kind = LedKind::Key;
    a.key = usage;
    return a;
}

KeyMapFile sample()
{
    KeyMapFile file;
    file.source = QStringLiteral("Calibration by the owner, each LED lit alone and confirmed by eye");
    file.firmware = QStringLiteral("2.3.88");
    // A refused answer would show at once: the tests compare what the map holds.
    static_cast<void>(file.map.assign(keyEntry(0x3A, 0x3A)));
    LedAssignment strip;
    strip.led = 0x82;
    strip.kind = LedKind::Strip;
    strip.stripPosition = 1;
    static_cast<void>(file.map.assign(strip));
    LedAssignment dark;
    dark.led = 0x00;
    dark.kind = LedKind::Unlit;
    static_cast<void>(file.map.assign(dark));
    return file;
}

QByteArray withLeds(const char *leds)
{
    return QByteArray(R"({"version":1,"device":"1b1c:2b0c","source":"s","leds":)") + leds + "}";
}

} // namespace

class TestKeyMapFile : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void aMapSurvivesBeingEncodedAndParsed()
    {
        const KeyMapFile file = sample();
        const auto parsed = parseKeyMap(encodeKeyMap(file));
        if (!parsed)
            QFAIL("the map was not parsed");
        QCOMPARE(parsed->source, file.source);
        QCOMPARE(parsed->firmware, file.firmware);
        QVERIFY(parsed->map.assignments() == file.map.assignments());
    }

    void aLedThatWasNotSeenIsNotInTheFile()
    {
        // "null until measured": an unmeasured LED has no entry at all, not a placeholder.
        const QByteArray bytes = encodeKeyMap(sample());
        QCOMPARE(bytes.count("\"led\""), 3);
        QVERIFY(!bytes.contains("null"));
    }

    void theFirmwareIsOptional()
    {
        const auto parsed = parseKeyMap(withLeds("[]"));
        if (!parsed)
            QFAIL("a map without firmware was refused");
        QVERIFY(parsed->firmware.isEmpty());
        QCOMPARE(parsed->map.assignedCount(), std::size_t{0});
    }

    void aMapWithoutSourceIsNotAMeasurementAndIsRefused()
    {
        QVERIFY(!parseKeyMap(R"({"version":1,"device":"1b1c:2b0c","leds":[]})"));
        QVERIFY(!parseKeyMap(R"({"version":1,"device":"1b1c:2b0c","source":"","leds":[]})"));
    }

    void anythingOutOfShapeIsRefused()
    {
        const auto refused = [](const QByteArray &json) { return !parseKeyMap(json).has_value(); };
        QVERIFY(refused(""));
        QVERIFY(refused("not json"));
        QVERIFY(refused("[]"));
        QVERIFY(refused(R"({"version":2,"device":"1b1c:2b0c","source":"s","leds":[]})"));
        QVERIFY(refused(R"({"version":1,"device":"1b1c:2b18","source":"s","leds":[]})")); // the Stream Deck part
        QVERIFY(refused(R"({"version":1,"device":"1b1c:2b0c","source":"s","leds":[],"extra":1})"));
        QVERIFY(refused(R"({"version":1,"device":"1b1c:2b0c","source":7,"leds":[]})"));
        QVERIFY(refused(R"({"version":1,"device":"1b1c:2b0c","source":"a\nb","leds":[]})")); // a control character
        QVERIFY(refused(R"({"version":1,"device":"1b1c:2b0c","source":"s","firmware":5,"leds":[]})"));
        QVERIFY(refused(R"({"version":1,"device":"1b1c:2b0c","source":"s","leds":{}})"));
        QVERIFY(refused(withLeds("[5]")));
        QVERIFY(refused(withLeds(R"([{"led":1}])")));
        QVERIFY(refused(withLeds(R"([{"led":1,"kind":"lamp"}])")));
        QVERIFY(refused(withLeds(R"([{"led":1,"kind":"unlit","key":4}])")));        // a detail the kind does not take
        QVERIFY(refused(withLeds(R"([{"led":1,"kind":"key"}])")));                   // the key is missing
        QVERIFY(refused(withLeds(R"([{"led":1,"kind":"key","key":4,"position":1}])")));
    }

    void numbersMustBeWholeAndInRange()
    {
        const auto refused = [](const char *leds) { return !parseKeyMap(withLeds(leds)).has_value(); };
        QVERIFY(refused(R"([{"led":"1","kind":"unlit"}])"));
        QVERIFY(refused(R"([{"led":1.5,"kind":"unlit"}])"));
        QVERIFY(refused(R"([{"led":-1,"kind":"unlit"}])"));
        QVERIFY(refused(R"([{"led":146,"kind":"unlit"}])"));                // beyond the 146 LEDs of the frame
        QVERIFY(refused(R"([{"led":1,"kind":"key","key":4.5}])"));
        QVERIFY(refused(R"([{"led":1,"kind":"key","key":3}])"));            // 3 is not a key
        QVERIFY(refused(R"([{"led":1,"kind":"strip","position":0}])"));     // sections count from 1
        QVERIFY(refused(R"([{"led":1,"kind":"strip","position":65}])"));
        QVERIFY(refused(R"([{"led":1,"kind":"strip","position":1e99}])"));
    }

    void aLedListedTwiceOrAKeyLitTwiceIsRefused()
    {
        QVERIFY(!parseKeyMap(withLeds(R"([{"led":1,"kind":"unlit"},{"led":1,"kind":"unlit"}])")));
        QVERIFY(!parseKeyMap(withLeds(R"([{"led":1,"kind":"key","key":4},{"led":2,"kind":"key","key":4}])")));
        // Two different LEDs for two different keys are fine.
        QVERIFY(parseKeyMap(withLeds(R"([{"led":1,"kind":"key","key":4},{"led":2,"kind":"key","key":5}])")));
    }

    void moreEntriesThanLedsAreRefusedAndSoIsAHugeFile()
    {
        QByteArray many = "[";
        for (int i = 0; i < 147; ++i)
            many += (i != 0 ? "," : "") + QByteArray(R"({"led":0,"kind":"unlit"})");
        many += "]";
        QVERIFY(!parseKeyMap(withLeds(many)));
        QVERIFY(!parseKeyMap(QByteArray(71680, ' ')));
    }

    void aLongSourceIsCutWhenWrittenSoTheReaderStillAcceptsIt()
    {
        KeyMapFile file = sample();
        file.source = QString(5000, QLatin1Char('x'));
        const auto parsed = parseKeyMap(encodeKeyMap(file));
        if (!parsed)
            QFAIL("the map was not parsed");
        QCOMPARE(parsed->source.size(), MaxKeyMapSource);
    }

    void theFileIsWrittenAsideAndReadBackAndAMissingOrBrokenOneIsNothing()
    {
        const QTemporaryDir dir;
        const QString path = dir.filePath("later/keymap.json");
        QString why;
        QVERIFY(!readKeyMap(path, &why));
        QVERIFY(!why.isEmpty());
        const KeyMapFile file = sample();
        QVERIFY(writeKeyMap(path, file)); // creates the folder
        const auto back = readKeyMap(path);
        if (!back)
            QFAIL("the map was not read back");
        QVERIFY(back->map.assignments() == file.map.assignments());
        QFile broken(path);
        QVERIFY(broken.open(QIODevice::WriteOnly | QIODevice::Truncate));
        broken.write("{");
        broken.close();
        QVERIFY(!readKeyMap(path));
    }

    void theReasonForARefusalNeverQuotesTheFile()
    {
        QString why;
        QVERIFY(!parseKeyMap(R"({"version":1,"device":"1b1c:2b0c","source":"SECRET-TEXT","leds":{}})", &why));
        QVERIFY(!why.isEmpty());
        QVERIFY(!why.contains(QStringLiteral("SECRET-TEXT")));
    }
};

QTEST_GUILESS_MAIN(TestKeyMapFile)
#include "test_keymapfile.moc"
