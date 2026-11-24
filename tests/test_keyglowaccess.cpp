// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

// The choice "the keys glow under the fingers" (key-glow.json): whether it is on and in which colour. The glow reads the key presses, so it is off unless the
// person made the choice in Bosun, and the file is read like any file from disk, as untrusted.
#include "KeyGlowAccess.h"
#include "KeySignalsAccess.h"

#include <QFile>
#include <QTemporaryDir>
#include <QTest>

using namespace galleond;
using galleon::keyboard::Rgb;

namespace {

QByteArray quoted(const QByteArray &text)
{
    return QByteArray(1, '"') + text + QByteArray(1, '"');
}

// A key-glow.json with this allow and this colour (both as JSON text).
QByteArray choiceFile(const QByteArray &allow, const QByteArray &colour)
{
    return QByteArray(R"({"version":1,"allow":)") + allow + R"(,"colour":)" + colour + "}";
}

} // namespace

class TestKeyGlowAccess : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void withoutAFileTheGlowIsOffAndCyan()
    {
        const QTemporaryDir dir;
        const QString path = dir.filePath("key-glow.json");
        QVERIFY(!keyGlowAllowed(path));
        QVERIFY(!readKeyGlowChoice(path).has_value());
        QCOMPARE(keyGlowChoice(path), (KeyGlowChoice{false, galleon::keyboard::DefaultGlowColour}));
        QCOMPARE(galleon::keyboard::DefaultGlowColour, (Rgb{0, 200, 255}));
    }

    void whatIsWrittenIsWhatIsReadBack()
    {
        const QTemporaryDir dir;
        const QString path = dir.filePath("key-glow.json");
        const KeyGlowChoice orange{true, Rgb{255, 152, 0}};
        QVERIFY(writeKeyGlowChoice(path, orange));
        QCOMPARE(keyGlowChoice(path), orange);
        QVERIFY(keyGlowAllowed(path));
        const KeyGlowChoice off{false, Rgb{1, 2, 3}};
        QVERIFY(writeKeyGlowChoice(path, off));
        QCOMPARE(keyGlowChoice(path), off); // off keeps its colour, so the choice of a colour is not lost when it is switched off
        QVERIFY(!keyGlowAllowed(path));
    }

    void aColourIsWrittenAsHashAndSixLowerCaseDigits()
    {
        QCOMPARE(keyGlowColourText(Rgb{0, 200, 255}), QStringLiteral("#00c8ff"));
        QCOMPARE(keyGlowColourText(Rgb{255, 0, 7}), QStringLiteral("#ff0007"));
        QCOMPARE(keyGlowColourText(Rgb{0, 0, 0}), QStringLiteral("#000000"));
        QCOMPARE(encodeKeyGlowChoice({true, Rgb{255, 152, 0}}).contains("#ff9800"), true);
    }

    void aColourIsReadBackOnlyFromExactlyThatForm_data()
    {
        QTest::addColumn<QString>("text");
        QTest::addColumn<bool>("good");
        QTest::newRow("lower case") << "#00c8ff" << true;
        QTest::newRow("upper case") << "#00C8FF" << true;
        QTest::newRow("black") << "#000000" << true;
        QTest::newRow("no hash") << "00c8ff" << false;
        QTest::newRow("three digits") << "#0cf" << false;
        QTest::newRow("seven digits") << "#00c8ff0" << false;
        QTest::newRow("not hexadecimal") << "#00c8fg" << false;
        QTest::newRow("a name") << "cyan" << false;
        QTest::newRow("a space") << "#00c8ff " << false;
        QTest::newRow("a sign") << "#+0c8ff" << false;
        QTest::newRow("empty") << "" << false;
    }
    void aColourIsReadBackOnlyFromExactlyThatForm()
    {
        QFETCH(const QString, text);
        QFETCH(const bool, good);
        QCOMPARE(parseKeyGlowColour(text).has_value(), good);
    }

    void aColourReadBackIsTheColourWritten()
    {
        for (const Rgb colour : {Rgb{0, 0, 0}, Rgb{255, 255, 255}, Rgb{18, 52, 86}, Rgb{255, 152, 0}}) {
            const auto back = parseKeyGlowColour(keyGlowColourText(colour));
            QVERIFY(back.has_value());
            QCOMPARE(back.value_or(Rgb{1, 2, 3}), colour);
        }
    }

    void aDamagedOrForeignFileMeansOffAndNothingIsTakenFromAPartOfIt_data()
    {
        QTest::addColumn<QByteArray>("bytes");
        const QByteArray hash(1, '#');
        QTest::newRow("cut") << (QByteArray(R"({"version":1,"allow":true,"colour":)") + quoted(hash + "ff98").chopped(1));
        QTest::newRow("a string for allow") << choiceFile(quoted("yes"), quoted(hash + "ff9800"));
        QTest::newRow("another version") << QByteArray(R"({"version":2,"allow":true,"colour":)") + quoted(hash + "ff9800") + "}";
        QTest::newRow("no colour") << QByteArray(R"({"version":1,"allow":true})");
        QTest::newRow("an extra key") << (QByteArray(R"({"version":1,"allow":true,"fade":9,"colour":)") + quoted(hash + "ff9800") + "}");
        QTest::newRow("a colour that is a number") << QByteArray(R"({"version":1,"allow":true,"colour":16750592})");
        QTest::newRow("a colour that is not a colour") << choiceFile("true", quoted("orange"));
        QTest::newRow("a good allow with a bad colour") << choiceFile("true", quoted(hash + "ff98"));
        QTest::newRow("the shape of key-signals.json") << QByteArray(R"({"version":1,"allow":true})");
        QTest::newRow("too large") << (QByteArray(400, ' ') + choiceFile("true", quoted(hash + "ff9800"))); // good, but padded beyond what is read
        QTest::newRow("empty") << QByteArray();
    }
    void aDamagedOrForeignFileMeansOffAndNothingIsTakenFromAPartOfIt()
    {
        QFETCH(const QByteArray, bytes);
        const QTemporaryDir dir;
        const QString path = dir.filePath("key-glow.json");
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(bytes);
        file.close();
        QString why;
        QVERIFY(!readKeyGlowChoice(path, &why).has_value());
        QVERIFY(!why.isEmpty());
        QCOMPARE(keyGlowChoice(path), KeyGlowChoice{}); // off, and the default colour: not the allow or the colour that a damaged file might hold
        QVERIFY(!keyGlowAllowed(path));
    }

    void theGlowAndTheSignalsHaveTheirOwnFilesAndOneDoesNotSwitchTheOtherOn()
    {
        QVERIFY(keyGlowAccessPath().endsWith("/galleoncore/key-glow.json"));
        QVERIFY(keySignalsAccessPath().endsWith("/galleoncore/key-signals.json"));
        QVERIFY(keyGlowAccessPath() != keySignalsAccessPath());
        const QTemporaryDir dir;
        QVERIFY(writeKeySignalsAccess(dir.filePath("key-signals.json"), true));
        QVERIFY(!keyGlowAllowed(dir.filePath("key-glow.json"))); // another file: nothing was said about the glow
    }
};

QTEST_APPLESS_MAIN(TestKeyGlowAccess)
#include "test_keyglowaccess.moc"
