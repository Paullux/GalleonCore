// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

// The key calibration of Bosun: off until asked, one LED lit at a time, the answers saved, no key kept. The lights are a fake:
// no daemon and no keyboard are involved.

#include "KeyCalibration.h"

#include <KeyMapFile.h>

#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <vector>

using namespace bosun;

namespace {

struct FakeLights final : CalibrationLights
{
    bool isReady = true;
    QString refusal;
    std::vector<int> ledsLit;  // the LEDs lit alone, in order
    int allCalls = 0;
    [[nodiscard]] bool ready() const override { return isReady; }
    QString showLed(int led, int /*red*/, int /*green*/, int /*blue*/) override
    {
        ledsLit.push_back(led);
        return refusal;
    }
    QString showAll(int /*red*/, int /*green*/, int /*blue*/) override
    {
        ++allCalls;
        return {};
    }
};

// What Qt reports as nativeScanCode for the key A, on this system (XKB keycode on Linux, the scan code on Windows), and for B.
#ifdef Q_OS_WIN
constexpr unsigned CodeA = 0x1E;
constexpr unsigned CodeB = 0x30;
#else
constexpr unsigned CodeA = 38;
constexpr unsigned CodeB = 56;
#endif
constexpr unsigned UnknownCode = 7; // not a key on either system

} // namespace

class TestKeyCalibration : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void everyKeyIsIgnoredUntilTheCalibrationIsBegun()
    {
        const QTemporaryDir dir;
        FakeLights lights;
        KeyCalibration calibration(lights, dir.filePath("keymap.json"));
        QVERIFY(!calibration.active());
        calibration.handleKey(CodeA);
        calibration.answerNone();
        calibration.skip();
        QVERIFY(lights.ledsLit.empty()); // nothing was lit, nothing recorded
        QVERIFY(!QFile::exists(dir.filePath("keymap.json")));
        QCOMPARE(calibration.passed(), 0);
    }

    void itNeedsTheLiveLightingToBeOn()
    {
        const QTemporaryDir dir;
        FakeLights lights;
        lights.isReady = false;
        KeyCalibration calibration(lights, dir.filePath("keymap.json"));
        QVERIFY(!calibration.begin(false).isEmpty());
        QVERIFY(!calibration.active());
        QVERIFY(lights.ledsLit.empty());
    }

    void beginningLightsTheFirstLedAlone()
    {
        const QTemporaryDir dir;
        FakeLights lights;
        KeyCalibration calibration(lights, dir.filePath("keymap.json"));
        QVERIFY(calibration.begin(false).isEmpty());
        QVERIFY(calibration.active());
        QCOMPARE(lights.ledsLit, (std::vector<int>{0}));
        QCOMPARE(calibration.currentLed(), 0);
        QVERIFY(!calibration.begin(false).isEmpty()); // already running
    }

    void aKeyPressedAnswersTheLedAndTheNextOneIsLit()
    {
        const QTemporaryDir dir;
        FakeLights lights;
        KeyCalibration calibration(lights, dir.filePath("keymap.json"));
        QVERIFY(calibration.begin(false).isEmpty());
        calibration.handleKey(CodeA);
        QCOMPARE(calibration.currentLed(), 1);
        QCOMPARE(lights.ledsLit, (std::vector<int>{0, 1}));
        // The answer is in the file at once.
        const auto saved = galleond::readKeyMap(dir.filePath("keymap.json"));
        QVERIFY(saved.has_value());
        QCOMPARE(saved.value_or(galleond::KeyMapFile{}).map.ledForKey(0x04), std::optional<std::uint16_t>(0));
    }

    void aKeyThatIsNotKnownOrAlreadyUsedChangesNothing()
    {
        const QTemporaryDir dir;
        FakeLights lights;
        KeyCalibration calibration(lights, dir.filePath("keymap.json"));
        QVERIFY(calibration.begin(false).isEmpty());
        calibration.handleKey(UnknownCode);
        QCOMPARE(calibration.currentLed(), 0);
        calibration.handleKey(CodeA);
        calibration.handleKey(CodeA); // A again for LED 1
        QCOMPARE(calibration.currentLed(), 1);
        QVERIFY(calibration.message().contains(QStringLiteral("LED 0")));
        calibration.handleKey(CodeB);
        QCOMPARE(calibration.currentLed(), 2);
    }

    void theMessageNamesTheKeyThatWasRecordedSoASlipIsSeen()
    {
        const QTemporaryDir dir;
        FakeLights lights;
        KeyCalibration calibration(lights, dir.filePath("keymap.json"));
        QVERIFY(calibration.begin(false).isEmpty());
        calibration.handleKey(CodeA);
        // The name is that of a QWERTY keyboard (the key at the A of a QWERTY), and the message says so: on an AZERTY it is engraved Q.
        QCOMPARE(calibration.message(), QStringLiteral("LED 0 = A (as named on a QWERTY keyboard)."));
        calibration.handleKey(CodeB);
        QCOMPARE(calibration.message(), QStringLiteral("LED 1 = B (as named on a QWERTY keyboard).")); // replaced by the next, not added to it
        calibration.answerNamedKey(0xFF01);
        QCOMPARE(calibration.message(), QStringLiteral("LED 2 = Fn.")); // a key that is the same on every layout: no remark
    }

    void aKeyTheSystemKeepsCanBeChosenFromTheListInsteadOfPressed()
    {
        const QTemporaryDir dir;
        FakeLights lights;
        KeyCalibration calibration(lights, dir.filePath("keymap.json"));
        QVERIFY(calibration.begin(false).isEmpty());
        const QVariantList list = KeyCalibration::specialKeys();
        QVERIFY(list.size() >= 9);
        QSet<int> usages;
        for (const QVariant &entry : list) {
            const QVariantMap map = entry.toMap();
            QVERIFY(!map.value(QStringLiteral("label")).toString().isEmpty());
            const int usage = map.value(QStringLiteral("usage")).toInt();
            QVERIFY(!usages.contains(usage)); // no key offered twice
            usages.insert(usage);
        }
        QVERIFY(usages.contains(0xFF01)); // Fn
        QVERIFY(usages.contains(0xFF02)); // Copilot
        calibration.answerNamedKey(0xFF02);
        QCOMPARE(calibration.currentLed(), 1);
        QVERIFY(calibration.message().contains(QStringLiteral("Copilot")));
        const auto saved = galleond::readKeyMap(dir.filePath("keymap.json"));
        QVERIFY(saved.has_value());
        QCOMPARE(saved.value_or(galleond::KeyMapFile{}).map.ledForKey(0xFF02), std::optional<std::uint16_t>(0));
    }

    void aChosenKeyThatIsTakenOrNotAKeyIsNotRecorded()
    {
        const QTemporaryDir dir;
        FakeLights lights;
        KeyCalibration calibration(lights, dir.filePath("keymap.json"));
        QVERIFY(calibration.begin(false).isEmpty());
        calibration.answerNamedKey(0x46);
        QCOMPARE(calibration.currentLed(), 1);
        calibration.answerNamedKey(0x46); // Print Screen already belongs to LED 0
        QCOMPARE(calibration.currentLed(), 1);
        QVERIFY(calibration.message().contains(QStringLiteral("LED 0")));
        calibration.answerNamedKey(3);     // not a key
        calibration.answerNamedKey(-1);
        calibration.answerNamedKey(70000);
        QCOMPARE(calibration.currentLed(), 1);
        QCOMPARE(calibration.passed(), 1);
    }

    void choosingAKeyDoesNothingWhenTheCalibrationIsNotRunning()
    {
        const QTemporaryDir dir;
        FakeLights lights;
        KeyCalibration calibration(lights, dir.filePath("keymap.json"));
        calibration.answerNamedKey(0x46);
        QCOMPARE(calibration.passed(), 0);
        QVERIFY(!QFile::exists(dir.filePath("keymap.json")));
    }

    void nothingLitSkipAndBackWork()
    {
        const QTemporaryDir dir;
        FakeLights lights;
        KeyCalibration calibration(lights, dir.filePath("keymap.json"));
        QVERIFY(calibration.begin(false).isEmpty());
        calibration.answerNone();
        calibration.skip();
        QCOMPARE(calibration.currentLed(), 2);
        QCOMPARE(calibration.skipped(), 1);
        calibration.back();
        QCOMPARE(calibration.currentLed(), 1);
        calibration.back();
        QCOMPARE(calibration.currentLed(), 0);
        calibration.back(); // the first question: nothing more to undo
        QCOMPARE(calibration.currentLed(), 0);
        calibration.answerStrip(0); // a section is counted from 1
        QCOMPARE(calibration.currentLed(), 0);
        calibration.answerStrip(2);
        QCOMPARE(calibration.currentLed(), 1);
    }

    void aLaterBeginContinuesTheEarlierAnswersUnlessStartingOver()
    {
        const QTemporaryDir dir;
        FakeLights lights;
        {
            KeyCalibration first(lights, dir.filePath("keymap.json"));
            QVERIFY(first.begin(false).isEmpty());
            first.handleKey(CodeA);
            first.end();
        }
        FakeLights again;
        KeyCalibration second(again, dir.filePath("keymap.json"));
        QVERIFY(second.begin(false).isEmpty());
        QCOMPARE(second.currentLed(), 1); // LED 0 is known
        second.end();
        QVERIFY(second.begin(true).isEmpty());
        QCOMPARE(second.currentLed(), 0); // starting over
    }

    void endingGivesTheColourBackAndStopsReadingKeys()
    {
        const QTemporaryDir dir;
        FakeLights lights;
        KeyCalibration calibration(lights, dir.filePath("keymap.json"));
        QVERIFY(calibration.begin(false).isEmpty());
        calibration.end();
        QVERIFY(!calibration.active());
        QCOMPARE(lights.allCalls, 1);
        const auto lit = lights.ledsLit.size();
        calibration.handleKey(CodeA);
        QCOMPARE(lights.ledsLit.size(), lit); // ignored
        QCOMPARE(calibration.passed(), 0);
    }

    void passingEveryLedFinishesAndGivesTheColourBack()
    {
        const QTemporaryDir dir;
        FakeLights lights;
        KeyCalibration calibration(lights, dir.filePath("keymap.json"));
        QVERIFY(calibration.begin(false).isEmpty());
        for (int i = 0; i < KeyCalibration::total(); ++i)
            calibration.answerNone();
        QVERIFY(calibration.finished());
        QCOMPARE(calibration.currentLed(), -1);
        QCOMPARE(lights.allCalls, 1);
        QCOMPARE(calibration.passed(), KeyCalibration::total());
    }

    void aLightingRefusalIsShownAndNothingIsSaved()
    {
        const QTemporaryDir dir;
        FakeLights lights;
        lights.refusal = QStringLiteral("the daemon is not running");
        KeyCalibration calibration(lights, dir.filePath("keymap.json"));
        QVERIFY(calibration.begin(false).isEmpty());
        QCOMPARE(calibration.message(), QStringLiteral("the daemon is not running"));
    }

    void noKeyPressIsKeptAnywhere()
    {
        // The only thing the calibration writes is the map: LED to key answers, no other file and no history of presses.
        const QTemporaryDir dir;
        FakeLights lights;
        KeyCalibration calibration(lights, dir.filePath("keymap.json"));
        QVERIFY(calibration.begin(false).isEmpty());
        calibration.handleKey(CodeA);
        calibration.handleKey(CodeB);
        calibration.handleKey(UnknownCode);
        QFile file(dir.filePath("keymap.json"));
        QVERIFY(file.open(QIODevice::ReadOnly));
        const QByteArray text = file.readAll();
        QVERIFY(!text.contains("time"));
        QVERIFY(!text.contains("order"));
        QVERIFY(!text.contains(QByteArray::number(UnknownCode) + ","));
        QCOMPARE(QDir(dir.path()).entryList(QDir::Files).size(), 1); // keymap.json only
    }
};

QTEST_GUILESS_MAIN(TestKeyCalibration)
#include "test_keycalibration.moc"
