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

    void theColoursOfTheRingsAreWrittenAndReadBackTheSameWhetherChosenOrByDefault()
    {
        const QTemporaryDir dir;
        const QString path = dir.filePath("dial-rings.json");
        const RingSettings both{.lit = true, .level = 600, .left = 0x1e88e5U, .right = 0xffd600U};
        QVERIFY(writeRingSettings(path, both));
        QCOMPARE(readRingSettings(path).value_or(RingSettings{.lit = false, .level = 0}), both);
        const RingSettings onlyLeft{.lit = true, .level = 600, .left = 0x43a047U, .right = std::nullopt};
        QVERIFY(writeRingSettings(path, onlyLeft));
        QCOMPARE(readRingSettings(path).value_or(RingSettings{.lit = false, .level = 0}), onlyLeft);
        // The colour written has all of its six digits, in lower case: black and a dark blue are not shortened.
        QVERIFY(encodeRingSettings({.lit = true, .level = 1000, .left = 0x000000U, .right = 0x0000ffU}).contains("\"left\": \"#000000\""));
        QVERIFY(encodeRingSettings({.lit = true, .level = 1000, .left = 0x000000U, .right = 0x0000ffU}).contains("\"right\": \"#0000ff\""));
        QVERIFY(encodeRingSettings({}).contains("\"left\": \"default\""));
    }

    void aFileOfBeforeTheColoursIsStillReadAndMeansTheDefaultForBothRings()
    {
        const auto old = parseRingSettings(R"({"version":1,"lit":false,"level":400})");
        QVERIFY(old.has_value());
        // A marker that no real colour has: if a colour were read, it would differ from the "default" (nothing) that is expected.
        const RingSettings read = old.value_or(RingSettings{.lit = true, .level = 0, .left = 1U, .right = 1U});
        QCOMPARE(read, (RingSettings{.lit = false, .level = 400}));
        QVERIFY(!read.left.has_value());
        QVERIFY(!read.right.has_value());
        // Version 1 has the three keys and no more: the colours do not slip into it.
        QVERIFY(!parseRingSettings(R"({"version":1,"lit":true,"level":1000,"left":"default","right":"default"})").has_value());
    }

    void aFileWithColoursThatIsNotExactlyRightIsRefusedWithAReason()
    {
        const QList<QByteArray> bad = {
            R"({"version":2,"lit":true,"level":1000})",                                              // version 2 without the colours
            R"({"version":2,"lit":true,"level":1000,"left":"default"})",                              // one colour missing
            R"({"version":2,"lit":true,"level":1000,"left":"default","right":"default","x":1})",      // a key too many
            R"({"version":3,"lit":true,"level":1000,"left":"default","right":"default"})",            // a version that does not exist
            R"({"version":2,"lit":true,"level":1000,"left":"red","right":"default"})",                // a name is not a colour
            R"({"version":2,"lit":true,"level":1000,"left":"#fff","right":"default"})",               // the short form
            R"({"version":2,"lit":true,"level":1000,"left":"#ggg000","right":"default"})",            // not hexadecimal
            R"({"version":2,"lit":true,"level":1000,"left":"#1e88e5 ","right":"default"})",           // a space after
            R"({"version":2,"lit":true,"level":1000,"left":"1e88e5","right":"default"})",             // no #
            R"({"version":2,"lit":true,"level":1000,"left":"#1e88e5ff","right":"default"})",          // too long
            R"({"version":2,"lit":true,"level":1000,"left":"Default","right":"default"})",            // the word is exactly "default"
            R"({"version":2,"lit":true,"level":1000,"left":null,"right":"default"})",                 // not a string
            R"({"version":2,"lit":true,"level":1000,"left":123456,"right":"default"})",               // a number is not a colour
            R"({"version":2,"lit":true,"level":1000,"left":"default","right":["#ffffff"]})",          // a list is not a colour
        };
        for (const QByteArray &bytes : bad) {
            QString why;
            QVERIFY2(!parseRingSettings(bytes, &why).has_value(), bytes.constData());
            QVERIFY2(!why.isEmpty(), bytes.constData());
        }
    }

    void aColourIsEitherTheWordDefaultOrSixDigitsAfterAHashAndNothingElse()
    {
        std::optional<std::uint32_t> choice = 0x123456U;
        QVERIFY(parseRingColour(QStringLiteral("#1E88e5"), &choice)); // either case is read ...
        QCOMPARE(choice.value_or(0), 0x1e88e5U);
        QCOMPARE(ringColourText(choice), QStringLiteral("#1e88e5")); // ... and written in lower case
        QVERIFY(parseRingColour(QStringLiteral("default"), &choice));
        QVERIFY(!choice.has_value());
        choice = 0x654321U;
        for (const QString &text : {QString(), QStringLiteral("#"), QStringLiteral("#12345"), QStringLiteral("#1234567"), QStringLiteral(" #123456"),
                                    QStringLiteral("#12345g"), QStringLiteral("red"), QStringLiteral("DEFAULT"), QStringLiteral("default ")}) {
            QVERIFY2(!parseRingColour(text, &choice), qPrintable(text));
            QCOMPARE(choice.value_or(0), 0x654321U); // refused: what was there stays
        }
        QVERIFY(parseRingColour(QStringLiteral("#000000"), nullptr)); // the answer alone is enough
        QVERIFY(!parseRingColour(QStringLiteral("x"), nullptr));
    }

    void aRingStartsFromTheColourThatWasChosenElseFromTheThemeOnTheLeftAndWhiteOnTheRight()
    {
        QCOMPARE(leftRingBase(0xe53935U, {}), 0xe53935U);
        QCOMPARE(rightRingBase({}), 0xffffffU);
        const RingSettings chosen{.lit = true, .level = 1000, .left = 0x1e88e5U, .right = 0xffd600U};
        QCOMPARE(leftRingBase(0xe53935U, chosen), 0x1e88e5U);
        QCOMPARE(rightRingBase(chosen), 0xffd600U);
        QCOMPARE(leftRingBase(0, chosen), 0x1e88e5U); // with no theme, a colour that was chosen still shows
        QCOMPARE(leftRingBase(0, {}), 0U);            // and with none chosen the left ring stays dark without a theme, as always
    }

    void aColourThatWasChosenIsPutOutAndDimmedLikeAnyOther()
    {
        const RingSettings out{.lit = false, .level = 1000, .left = 0x1e88e5U, .right = 0xffd600U};
        QCOMPARE(ambientRingColour(leftRingBase(0xe53935U, out), out), 0U);
        QCOMPARE(ambientRingColour(rightRingBase(out), out), 0U);
        const RingSettings zero{.lit = true, .level = 0, .left = 0x1e88e5U, .right = 0xffd600U};
        QCOMPARE(ambientRingColour(leftRingBase(0xe53935U, zero), zero), 0U);
        const RingSettings half{.lit = true, .level = 500, .left = 0x1e88e5U, .right = 0xffd600U};
        QCOMPARE(ambientRingColour(leftRingBase(0xe53935U, half), half), 0x0f4473U);
        QCOMPARE(ambientRingColour(rightRingBase(half), half), 0x806b00U);
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

    // ---- the ring that answers an action of the person (the magenta of the wallpaper choice), in normal use
    void theAnswerFollowsTheLevelAndKeepsItsHue()
    {
        QCOMPARE(answerRingColour(0xff00ff, {}), std::uint32_t{0xff00ff});                                  // full level: as it is
        QCOMPARE(answerRingColour(0xff00ff, {.lit = true, .level = 500}), std::uint32_t{0x800080});          // half: 127.5 rounds up
        QCOMPARE(answerRingColour(0xff00ff, {.lit = true, .level = 700}), std::uint32_t{0xb300b3});          // 178.5 rounds up
    }

    void aLowLevelIsRaisedToTheMinimumSoTheAnswerStaysReadable()
    {
        const std::uint32_t atTheMinimum = ambientRingColour(0xff00ff, {.lit = true, .level = RingShownMinLevel});
        QCOMPARE(atTheMinimum, std::uint32_t{0x4d004d}); // 30 % of 255 is 76.5, 77
        QCOMPARE(answerRingColour(0xff00ff, {.lit = true, .level = 100}), atTheMinimum);
        QCOMPARE(answerRingColour(0xff00ff, {.lit = true, .level = 1}), atTheMinimum);
        QCOMPARE(answerRingColour(0xff00ff, {.lit = true, .level = RingShownMinLevel - 1}), atTheMinimum);
        QVERIFY(answerRingColour(0xff00ff, {.lit = true, .level = RingShownMinLevel + 100}) > atTheMinimum);
    }

    void withTheRingsPutOutOrAtZeroTheAnswerDoesNotRelightThem()
    {
        QCOMPARE(answerRingColour(0xff00ff, {.lit = false, .level = 1000}), std::uint32_t{0}); // put out, whatever the level kept in the file
        QCOMPARE(answerRingColour(0xff00ff, {.lit = false, .level = 0}), std::uint32_t{0});
        QCOMPARE(answerRingColour(0xff00ff, {.lit = true, .level = 0}), std::uint32_t{0});     // lit but at 0 %: the same
    }

    void theAnswerNeverGoesAboveItsOwnChannelAndNeverUnderTheMinimumOnceShown()
    {
        for (int level = 1; level <= RingLevelFull; ++level) {
            const std::uint32_t shown = answerRingColour(0xff00ff, {.lit = true, .level = level});
            QCOMPARE((shown >> 8) & 0xffU, std::uint32_t{0}); // green was 0 and stays 0
            QCOMPARE((shown >> 16) & 0xffU, shown & 0xffU);   // red and blue stay together
            QVERIFY(((shown >> 16) & 0xffU) >= 77U);          // never under 30 % of 255
        }
    }

    // ---- the green rain, the one exception
    void theRainIsAtLeastAtTheMinimumEvenWithTheRingsPutOutOrAtZero()
    {
        const std::uint32_t atTheMinimum = ambientRingColour(0x00d43a, {.lit = true, .level = RingShownMinLevel});
        QVERIFY(atTheMinimum != 0);
        QCOMPARE(floorRingColour(0x00d43a, {.lit = false, .level = 1000}), atTheMinimum); // put out: the rain relights them
        QCOMPARE(floorRingColour(0x00d43a, {.lit = false, .level = 0}), atTheMinimum);
        QCOMPARE(floorRingColour(0x00d43a, {.lit = true, .level = 0}), atTheMinimum);     // at 0 %: the same
        QCOMPARE(floorRingColour(0x00d43a, {.lit = true, .level = 100}), atTheMinimum);   // low: raised
    }

    void theRainFollowsTheLevelAboveTheMinimum()
    {
        QCOMPARE(floorRingColour(0x00d43a, {}), std::uint32_t{0x00d43a});
        QCOMPARE(floorRingColour(0x00d43a, {.lit = true, .level = 500}), ambientRingColour(0x00d43a, {.lit = true, .level = 500}));
    }
};

QTEST_GUILESS_MAIN(TestRingSettings)
#include "test_ringsettings.moc"
