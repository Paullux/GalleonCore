// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

// The settings of the rings of the dials: the file is read as untrusted input, and the colour that a ring shows is a pure function of its colour
// and the settings.
#include "RingSettings.h"

#include <QFile>
#include <QTemporaryDir>
#include <QTest>

using namespace galleond;

class TestRingSettings : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void aFileWrittenByTheCodeIsReadBackTheSame()
    {
        const QTemporaryDir dir;
        const QString path = dir.filePath("dial-rings.json");
        QVERIFY(writeRingSettings(path, {.lit = false, .level = 300}));
        QCOMPARE(readRingSettings(path).value_or(RingSettings{}), (RingSettings{.lit = false, .level = 300}));
        QVERIFY(writeRingSettings(path, {}));
        QCOMPARE(readRingSettings(path).value_or(RingSettings{.lit = false, .level = 0}), RingSettings{});
    }

    void nothingOutOfRangeIsWritten()
    {
        const QTemporaryDir dir;
        const QString path = dir.filePath("dial-rings.json");
        QVERIFY(!writeRingSettings(path, {.lit = true, .level = 1001}));
        QVERIFY(!writeRingSettings(path, {.lit = true, .level = -1}));
        QVERIFY(!QFile::exists(path));
    }

    void aFileThatIsNotExactlyRightIsRefusedWithAReason()
    {
        const QList<QByteArray> bad = {
            {},
            "not json",
            "[]",
            R"({"version":1,"lit":true})",                                  // a key missing
            R"({"version":1,"lit":true,"level":1000,"extra":1})",           // a key too many
            R"({"version":2,"lit":true,"level":1000})",                     // an unknown version
            R"({"version":1,"lit":"yes","level":1000})",                    // not a boolean
            R"({"version":1,"lit":1,"level":1000})",                        // a number is not a boolean
            R"({"version":1,"lit":true,"level":"1000"})",                   // a string is not a number
            R"({"version":1,"lit":true,"level":1000.5})",                   // not a whole number
            R"({"version":1,"lit":true,"level":1001})",                     // above the range
            R"({"version":1,"lit":true,"level":-1})",                       // below the range
            R"({"version":1,"lit":true,"level":1e99})",                     // far above
        };
        for (const QByteArray &bytes : bad) {
            QString why;
            QVERIFY2(!parseRingSettings(bytes, &why).has_value(), bytes.constData());
            QVERIFY2(!why.isEmpty(), bytes.constData());
        }
        QVERIFY(!parseRingSettings(QByteArray(MaxRingSettingsBytes + 1, ' ')).has_value()); // too large
    }

    void theEdgesOfTheRangeAreAccepted()
    {
        QCOMPARE(parseRingSettings(R"({"version":1,"lit":true,"level":0})").value_or(RingSettings{.lit = false, .level = 9}), (RingSettings{.lit = true, .level = 0}));
        QCOMPARE(parseRingSettings(R"({"version":1,"lit":false,"level":1000})").value_or(RingSettings{.lit = true, .level = 9}), (RingSettings{.lit = false, .level = 1000}));
    }

    void aRingThatIsPutOutShowsNoColourWhateverTheLevel()
    {
        QCOMPARE(ambientRingColour(0xe53935, {.lit = false, .level = 1000}), std::uint32_t{0});
        QCOMPARE(ambientRingColour(0xffffff, {.lit = false, .level = 0}), std::uint32_t{0});
    }

    void aFullLevelLeavesTheColourAsItIsAndHalfTheLevelHalvesEachChannelToTheNearest()
    {
        QCOMPARE(ambientRingColour(0xe53935, {}), std::uint32_t{0xe53935});
        QCOMPARE(ambientRingColour(0xe53935, {.lit = true, .level = 500}), std::uint32_t{0x731d1b}); // 229, 57, 53 -> 115, 29, 27 (114.5 rounds up)
        QCOMPARE(ambientRingColour(0xffffff, {.lit = true, .level = 0}), std::uint32_t{0});
    }

    void theColourNeverGoesAboveItsOwnChannelAndSteadilyGrowsWithTheLevel()
    {
        std::uint32_t previous = 0;
        for (int level = 0; level <= RingLevelFull; ++level) {
            const std::uint32_t shown = ambientRingColour(0x80ff10, {.lit = true, .level = level});
            QVERIFY(((shown >> 16) & 0xffU) <= 0x80U && ((shown >> 8) & 0xffU) <= 0xffU && (shown & 0xffU) <= 0x10U);
            QVERIFY(((shown >> 8) & 0xffU) >= ((previous >> 8) & 0xffU));
            previous = shown;
        }
    }

    // ---- the ring that answers an action of the person (the magenta of the wallpaper choice)
    void theAnswerFollowsTheLevelAndKeepsItsHue()
    {
        QCOMPARE(answerRingColour(0xff00ff, {}), std::uint32_t{0xff00ff});                                  // full level: as it is
        QCOMPARE(answerRingColour(0xff00ff, {.lit = true, .level = 500}), std::uint32_t{0x800080});          // half: 127.5 rounds up
        QCOMPARE(answerRingColour(0xff00ff, {.lit = true, .level = 700}), std::uint32_t{0xb300b3});          // 178.5 rounds up
    }

    void theAnswerNeverGoesUnderTheMinimumLevel()
    {
        const std::uint32_t atTheMinimum = ambientRingColour(0xff00ff, {.lit = true, .level = RingAnswerMinLevel});
        QCOMPARE(atTheMinimum, std::uint32_t{0x4d004d}); // 30 % of 255 is 76.5, 77
        QCOMPARE(answerRingColour(0xff00ff, {.lit = true, .level = 100}), atTheMinimum);
        QCOMPARE(answerRingColour(0xff00ff, {.lit = true, .level = 0}), atTheMinimum);
        // Just under and just over the minimum.
        QCOMPARE(answerRingColour(0xff00ff, {.lit = true, .level = RingAnswerMinLevel - 1}), atTheMinimum);
        QVERIFY(answerRingColour(0xff00ff, {.lit = true, .level = RingAnswerMinLevel + 100}) > atTheMinimum);
    }

    void withTheRingsPutOutTheAnswerIsAtTheMinimumAndNeverDark()
    {
        const std::uint32_t atTheMinimum = answerRingColour(0xff00ff, {.lit = true, .level = 0});
        QVERIFY(atTheMinimum != 0);
        QCOMPARE(answerRingColour(0xff00ff, {.lit = false, .level = 1000}), atTheMinimum); // whatever the level kept in the file
        QCOMPARE(answerRingColour(0xff00ff, {.lit = false, .level = 0}), atTheMinimum);
    }

    void theAnswerNeverGoesAboveItsOwnChannelAndNeverBelowTheMinimumOfEachChannel()
    {
        for (int level = 0; level <= RingLevelFull; ++level) {
            const std::uint32_t shown = answerRingColour(0xff00ff, {.lit = true, .level = level});
            QCOMPARE((shown >> 8) & 0xffU, std::uint32_t{0}); // green was 0 and stays 0
            QCOMPARE((shown >> 16) & 0xffU, shown & 0xffU);   // red and blue stay together
            QVERIFY(((shown >> 16) & 0xffU) >= 77U);          // never under 30 % of 255
        }
    }
};

QTEST_GUILESS_MAIN(TestRingSettings)
#include "test_ringsettings.moc"
