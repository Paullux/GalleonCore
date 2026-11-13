// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

// keyboard-state.json: the strict format, the atomic write, and the publisher that writes it after a read or a
// confirmed write of the brightness. The controller here has an opener that never opens anything: its signals
// are emitted by hand, no keyboard is involved.

#include "KeyboardController.h"
#include "KeyboardState.h"
#include "KeyboardStatePublisher.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>
#include <QByteArray>
#include <QFileInfo>
#include <QRegularExpression>

using namespace galleond;

namespace {
KeyboardController::Opener neverOpens()
{
    return []() -> std::unique_ptr<galleon::Transport> { return nullptr; };
}
} // namespace

class TestKeyboardState : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void aStateSurvivesBeingEncodedAndParsed()
    {
        const KeyboardState state{.brightness = 700, .time = 1783938123};
        QCOMPARE(parseKeyboardState(encodeKeyboardState(state)), std::optional<KeyboardState>(state));
        const KeyboardState edge{.brightness = 0, .time = 0};
        QCOMPARE(parseKeyboardState(encodeKeyboardState(edge)), std::optional<KeyboardState>(edge));
        const KeyboardState top{.brightness = 1000, .time = 5};
        QCOMPARE(parseKeyboardState(encodeKeyboardState(top)), std::optional<KeyboardState>(top));
    }

    void aValueThatIsNotAStepOfOneHundredIsKeptAsItIs()
    {
        // Another program may have set 450: it is reported, and rounding is the reader's business.
        const KeyboardState state{.brightness = 450, .time = 10};
        QCOMPARE(parseKeyboardState(encodeKeyboardState(state)), std::optional<KeyboardState>(state));
    }

    void anythingThatIsNotExactlyTheThreeKeysWithWholeNumbersInRangeIsRefused()
    {
        const auto refused = [](const char *json) { return !parseKeyboardState(QByteArray(json)).has_value(); };
        QVERIFY(refused(""));
        QVERIFY(refused("not json"));
        QVERIFY(refused("[]"));
        QVERIFY(refused(R"({"version":1,"brightness":700})"));
        QVERIFY(refused(R"({"version":1,"brightness":700,"time":5,"extra":1})"));
        QVERIFY(refused(R"({"version":2,"brightness":700,"time":5})"));
        QVERIFY(refused(R"({"version":"1","brightness":700,"time":5})"));
        QVERIFY(refused(R"({"version":1,"brightness":-1,"time":5})"));
        QVERIFY(refused(R"({"version":1,"brightness":1001,"time":5})"));
        QVERIFY(refused(R"({"version":1,"brightness":70.5,"time":5})"));
        QVERIFY(refused(R"({"version":1,"brightness":"700","time":5})"));
        QVERIFY(refused(R"({"version":1,"brightness":700,"time":-1})"));
        QVERIFY(refused(R"({"version":1,"brightness":700,"time":1.5})"));
        QVERIFY(refused(R"({"version":1,"brightness":700,"time":99999999999999})"));
        QVERIFY(refused(R"({"version":1,"brightness":null,"time":5})"));
    }

    void aFileOverTheSizeLimitIsRefusedWithoutBeingTrusted()
    {
        QByteArray big = encodeKeyboardState({.brightness = 1, .time = 1});
        big.append(QByteArray(static_cast<int>(MaxKeyboardStateBytes), ' '));
        QVERIFY(!parseKeyboardState(big));
    }

    void aStateOlderThanTheStartOfTheReaderIsNotFresh()
    {
        const KeyboardState state{.brightness = 300, .time = 100};
        QVERIFY(isFreshKeyboardState(state, 100));
        QVERIFY(isFreshKeyboardState(state, 50));
        QVERIFY(!isFreshKeyboardState(state, 101));
    }

    void theStateIsWrittenAtomicallyInANewFolderAndReadBack()
    {
        const QTemporaryDir dir;
        const QString path = dir.filePath("new/folder/keyboard-state.json");
        QVERIFY(writeKeyboardState(path, {.brightness = 200, .time = 7}));
        QCOMPARE(readKeyboardState(path), std::optional<KeyboardState>(KeyboardState{.brightness = 200, .time = 7}));
        QVERIFY(writeKeyboardState(path, {.brightness = 900, .time = 8}));
        QCOMPARE(readKeyboardState(path).value_or(KeyboardState{}).brightness, 900);
        // Nothing else is left beside it: the temporary file was renamed, not abandoned.
        QCOMPARE(QDir(QFileInfo(path).absolutePath()).entryList(QDir::Files).size(), 1);
    }

    void aMissingOrGarbageFileReadsAsNothing()
    {
        const QTemporaryDir dir;
        QVERIFY(!readKeyboardState(dir.filePath("absent.json")));
        QFile garbage(dir.filePath("garbage.json"));
        QVERIFY(garbage.open(QIODevice::WriteOnly));
        garbage.write("{\"version\":1,");
        garbage.close();
        QVERIFY(!readKeyboardState(dir.filePath("garbage.json")));
    }

    void theDefaultPathIsNextToThePagesFileInTheConfigFolder()
    {
        QVERIFY(keyboardStatePath().endsWith(QStringLiteral("/galleoncore/keyboard-state.json")));
    }

    // ---- the publisher ------------------------------------------------------------------------------
    void aReadPublishesTheValueWithTheTimeOfTheClock()
    {
        const QTemporaryDir dir;
        const QString path = dir.filePath("state.json");
        KeyboardController controller(neverOpens());
        const KeyboardStatePublisher publisher(controller, path, [] { return qint64{1234}; });
        Q_EMIT controller.brightnessRead(450, true, {});
        QCOMPARE(readKeyboardState(path), std::optional<KeyboardState>(KeyboardState{.brightness = 450, .time = 1234}));
    }

    void aConfirmedWritePublishesTheValueWritten()
    {
        const QTemporaryDir dir;
        const QString path = dir.filePath("state.json");
        KeyboardController controller(neverOpens());
        const KeyboardStatePublisher publisher(controller, path, [] { return qint64{99}; });
        Q_EMIT controller.brightnessWritten(800, true, {});
        QCOMPARE(readKeyboardState(path).value_or(KeyboardState{}).brightness, 800);
    }

    void aFailedReadOrWriteLeavesTheLastRealValueInPlace()
    {
        const QTemporaryDir dir;
        const QString path = dir.filePath("state.json");
        KeyboardController controller(neverOpens());
        const KeyboardStatePublisher publisher(controller, path, [] { return qint64{1}; });
        Q_EMIT controller.brightnessRead(600, true, {});
        Q_EMIT controller.brightnessRead(0, false, QStringLiteral("no answer"));
        Q_EMIT controller.brightnessWritten(100, false, QStringLiteral("not confirmed"));
        QCOMPARE(readKeyboardState(path).value_or(KeyboardState{}).brightness, 600); // never replaced by a guess
    }

    void aFailedFirstReadWritesNoFileAtAll()
    {
        const QTemporaryDir dir;
        const QString path = dir.filePath("state.json");
        KeyboardController controller(neverOpens());
        const KeyboardStatePublisher publisher(controller, path, [] { return qint64{1}; });
        Q_EMIT controller.brightnessRead(0, false, QStringLiteral("could not be opened"));
        QVERIFY(!QFile::exists(path));
    }

    void aFolderThatCannotBeWrittenDoesNotStopTheDaemon()
    {
        const QTemporaryDir dir;
        QFile blocker(dir.filePath("file"));
        QVERIFY(blocker.open(QIODevice::WriteOnly));
        blocker.close();
        KeyboardController controller(neverOpens());
        const KeyboardStatePublisher publisher(controller, dir.filePath("file/state.json"), [] { return qint64{1}; });
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression("cannot write"));
        Q_EMIT controller.brightnessRead(300, true, {}); // must only warn
    }
};

QTEST_GUILESS_MAIN(TestKeyboardState)
#include "test_keyboardstate.moc"

#include <memory>
#include <optional>
