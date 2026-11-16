// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

// lighting-appearance.json: the strict format of the appearance of the live lighting and its atomic write. Pure files: no keyboard is opened.

#include "AppearanceFile.h"

#include <QFile>
#include <QTemporaryDir>
#include <QTest>

using namespace galleond;
using galleon::keyboard::Appearance;
using galleon::keyboard::BaseType;

namespace {

QByteArray withBase(const char *base, const char *overrides = "[]", const char *level = "1000")
{
    return QByteArray(R"({"version":1,"base":)") + base + R"(,"overrides":)" + overrides + R"(,"level":)" + level + "}";
}

constexpr const char *Black = R"({"type":"uniform","colour":[0,0,0]})";

} // namespace

class TestAppearanceFile : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void aUniformAppearanceSurvivesBeingEncodedAndParsed()
    {
        Appearance appearance;
        appearance.colour = {10, 20, 255};
        appearance.retouches = {{58, {255, 255, 255}}, {4, {0, 0, 255}}};
        appearance.level = 640;
        const auto parsed = parseAppearance(encodeAppearance(appearance));
        QVERIFY(parsed.has_value());
        QVERIFY(parsed.value_or(Appearance{}) == appearance);
    }

    void aRainbowAppearanceSurvivesBeingEncodedAndParsed()
    {
        Appearance appearance;
        appearance.base = BaseType::Rainbow;
        appearance.rainbowSpeed = 250;
        appearance.retouches = {{0, {1, 2, 3}}};
        const auto parsed = parseAppearance(encodeAppearance(appearance));
        QVERIFY(parsed.has_value());
        QVERIFY(parsed.value_or(Appearance{}) == appearance);
        QVERIFY(!encodeAppearance(appearance).contains("colour")); // a rainbow carries no colour
    }

    void theFixedRainbowAndTheDarkDefaultAreWrittenAsAppearancesToo()
    {
        // The simplest appearances are saved like the others: a still rainbow (speed 0) and a plain black base.
        Appearance rainbow;
        rainbow.base = BaseType::Rainbow;
        const auto parsedRainbow = parseAppearance(encodeAppearance(rainbow));
        QVERIFY(parsedRainbow.has_value());
        QCOMPARE(parsedRainbow.value_or(Appearance{}).rainbowSpeed, 0);
        const auto parsedDark = parseAppearance(encodeAppearance(Appearance{}));
        QVERIFY(parsedDark.has_value());
        QVERIFY(parsedDark.value_or(Appearance{}).retouches.empty());
    }

    void aSkyAppearanceSurvivesBeingEncodedAndParsedAndTakesNoParameter()
    {
        Appearance sky;
        sky.base = BaseType::Sky;
        sky.retouches = {{58, {255, 255, 255}}};
        const auto parsed = parseAppearance(encodeAppearance(sky));
        QVERIFY(parsed.has_value());
        QVERIFY(parsed.value_or(Appearance{}) == sky);
        QVERIFY(!parseAppearance(withBase(R"({"type":"sky","speed":0})", "[]", "1000")).has_value()); // nothing else is allowed in the base
        QVERIFY(!parseAppearance(withBase(R"({"type":"sky","colour":[0,0,0]})", "[]", "1000")).has_value());
        QVERIFY(parseAppearance(withBase(R"({"type":"sky"})", "[]", "1000")).has_value());
    }

    void anythingOutOfShapeIsRefused()
    {
        const auto refused = [](const QByteArray &json) { return !parseAppearance(json).has_value(); };
        QVERIFY(refused(""));
        QVERIFY(refused("not json"));
        QVERIFY(refused("[]"));
        QVERIFY(refused(R"({"version":2,"base":)" + QByteArray(Black) + R"(,"overrides":[],"level":1000})"));
        QVERIFY(refused(R"({"base":)" + QByteArray(Black) + R"(,"overrides":[],"level":1000})")); // no version
        QVERIFY(refused(R"({"version":1,"base":)" + QByteArray(Black) + R"(,"overrides":[],"level":1000,"extra":1})"));
        QVERIFY(refused(withBase("5")));
        QVERIFY(refused(withBase(R"({"type":"plasma"})")));
        QVERIFY(refused(withBase(R"({"type":"uniform"})")));                                  // no colour
        QVERIFY(refused(withBase(R"({"type":"uniform","colour":[0,0,0],"speed":1})")));      // a key the type does not take
        QVERIFY(refused(withBase(R"({"type":"rainbow","colour":[0,0,0]})")));                // a rainbow has a speed, not a colour
        QVERIFY(refused(withBase(R"({"type":"rainbow"})")));
        QVERIFY(refused(withBase(Black, "5")));
        QVERIFY(refused(withBase(Black, "[5]")));
    }

    void numbersMustBeWholeAndInRange()
    {
        const auto refused = [](const QByteArray &json) { return !parseAppearance(json).has_value(); };
        QVERIFY(refused(withBase(R"({"type":"uniform","colour":[256,0,0]})")));
        QVERIFY(refused(withBase(R"({"type":"uniform","colour":[0,-1,0]})")));
        QVERIFY(refused(withBase(R"({"type":"uniform","colour":[0,0]})")));
        QVERIFY(refused(withBase(R"({"type":"uniform","colour":[0,0,0,0]})")));
        QVERIFY(refused(withBase(R"({"type":"uniform","colour":[1.5,0,0]})")));
        QVERIFY(refused(withBase(R"({"type":"uniform","colour":["1",0,0]})")));
        QVERIFY(refused(withBase(R"({"type":"rainbow","speed":1001})")));
        QVERIFY(refused(withBase(R"({"type":"rainbow","speed":-1})")));
        QVERIFY(refused(withBase(R"({"type":"rainbow","speed":2.5})")));
        QVERIFY(refused(withBase(Black, "[]", "1001")));
        QVERIFY(refused(withBase(Black, "[]", "-1")));
        QVERIFY(refused(withBase(Black, "[]", "50.5")));
        QVERIFY(refused(withBase(Black, "[]", R"("1000")")));
        QVERIFY(refused(withBase(Black, "[[146,0,0,0]]")));        // beyond the 146 LEDs
        QVERIFY(refused(withBase(Black, "[[-1,0,0,0]]")));
        QVERIFY(refused(withBase(Black, "[[1,256,0,0]]")));
        QVERIFY(refused(withBase(Black, "[[1,0,0]]")));            // a LED and only two colours
        QVERIFY(refused(withBase(Black, "[[1,0,0,0,0]]")));
        QVERIFY(refused(withBase(Black, "[[1.5,0,0,0]]")));
        QVERIFY(parseAppearance(withBase(Black, "[[145,255,255,255]]")).has_value()); // the last LED, the biggest values
        QVERIFY(parseAppearance(withBase(R"({"type":"rainbow","speed":1000})", "[]", "0")).has_value()); // the bounds themselves
    }

    void aLedOverriddenTwiceIsRefused()
    {
        QVERIFY(!parseAppearance(withBase(Black, "[[5,1,1,1],[5,2,2,2]]")).has_value());
        QVERIFY(parseAppearance(withBase(Black, "[[5,1,1,1],[6,2,2,2]]")).has_value());
    }

    void moreOverridesThanLedsAreRefusedAndSoIsAHugeFile()
    {
        QByteArray many = "[";
        for (int i = 0; i < 147; ++i)
            many += (i != 0 ? "," : "") + QByteArray("[3,1,1,1]");
        many += "]";
        QVERIFY(!parseAppearance(withBase(Black, many)).has_value());
        QByteArray all = "[";
        for (int led = 0; led < 146; ++led)
            all += (led != 0 ? "," : "") + QByteArray("[") + QByteArray::number(led) + ",255,255,255]";
        all += "]";
        const QByteArray biggest = withBase(Black, all);
        QVERIFY2(biggest.size() <= MaxAppearanceBytes, "the biggest appearance has to fit in the size limit");
        QVERIFY(parseAppearance(biggest).has_value()); // 146 overrides are the most
        QVERIFY(!parseAppearance(QByteArray(20480, ' ')).has_value());
    }

    void theFileIsWrittenAsideAndReadBackAndAMissingOrBrokenOneIsNothing()
    {
        const QTemporaryDir dir;
        const QString path = dir.filePath("later/lighting-appearance.json");
        QString why;
        QVERIFY(!readAppearance(path, &why).has_value());
        QVERIFY(!why.isEmpty());
        Appearance appearance;
        appearance.base = BaseType::Rainbow;
        appearance.rainbowSpeed = 100;
        appearance.retouches = {{58, {255, 255, 255}}};
        QVERIFY(writeAppearance(path, appearance)); // creates the folder
        const auto back = readAppearance(path);
        QVERIFY(back.has_value());
        QVERIFY(back.value_or(Appearance{}) == appearance);
        QFile broken(path);
        QVERIFY(broken.open(QIODevice::WriteOnly | QIODevice::Truncate));
        broken.write("{");
        broken.close();
        QVERIFY(!readAppearance(path).has_value());
    }

    void anAppearanceThatIsNotValidIsNeverWritten()
    {
        const QTemporaryDir dir;
        const QString path = dir.filePath("lighting-appearance.json");
        Appearance wild;
        wild.level = 5000;
        QVERIFY(!writeAppearance(path, wild));
        QVERIFY(!QFile::exists(path));
    }

    void theReasonForARefusalNeverQuotesTheFile()
    {
        QString why;
        QVERIFY(!parseAppearance(R"({"version":1,"base":{"type":"SECRET-TEXT"},"overrides":[],"level":1000})", &why).has_value());
        QVERIFY(!why.isEmpty());
        QVERIFY(!why.contains(QStringLiteral("SECRET-TEXT")));
    }
};

QTEST_GUILESS_MAIN(TestAppearanceFile)
#include "test_appearancefile.moc"
