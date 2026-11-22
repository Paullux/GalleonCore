// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

// Putting the live lighting back when galleond starts: the two small files are untrusted input, and the decision is pure. The lighting is put back
// only when the person asked AND galleond ended properly with it on; any other state leaves it off. Nothing here touches a keyboard.
#include "LightingRestore.h"

#include <QFile>
#include <QTemporaryDir>
#include <QTest>

using namespace galleond;

namespace {

bool writeRaw(const QString &path, const QByteArray &bytes)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly | QIODevice::Truncate) && file.write(bytes) == bytes.size();
}

} // namespace

class TestLightingRestore : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void theChoiceOfThePersonIsReadBackAsItWasWritten()
    {
        for (const bool restore : {true, false}) {
            const auto read = parseLightingRestore(encodeLightingRestore(restore));
            QVERIFY(read.has_value());
            QCOMPARE(read.value_or(!restore), restore);
        }
    }

    void theMemoryOfGalleondIsReadBackAsItWasWritten()
    {
        for (const LightingMemory memory : {LightingMemory::Off, LightingMemory::Running, LightingMemory::CleanExit}) {
            const auto read = parseLightingMemory(encodeLightingMemory(memory));
            QVERIFY(read.has_value());
            QVERIFY(read.value_or(LightingMemory::Off) == memory && read.has_value());
        }
    }

    void aChoiceThatIsNotWellFormedIsIgnored_data()
    {
        QTest::addColumn<QByteArray>("bytes");
        QTest::newRow("empty") << QByteArray();
        QTest::newRow("not json") << QByteArray("restore");
        QTest::newRow("an array") << QByteArray("[true]");
        QTest::newRow("a string instead of a boolean") << QByteArray(R"({"version":1,"restore":"true"})");
        QTest::newRow("a number instead of a boolean") << QByteArray(R"({"version":1,"restore":1})");
        QTest::newRow("another version") << QByteArray(R"({"version":2,"restore":true})");
        QTest::newRow("a missing key") << QByteArray(R"({"version":1})");
        QTest::newRow("an extra key") << QByteArray(R"({"version":1,"restore":true,"on":true})");
        QTest::newRow("too large") << (QByteArray(R"({"version":1,"restore":true,"x":")") + QByteArray(400, 'a') + "\"}");
    }
    void aChoiceThatIsNotWellFormedIsIgnored()
    {
        QFETCH(const QByteArray, bytes);
        QVERIFY(!parseLightingRestore(bytes).has_value());
    }

    void aMemoryThatIsNotWellFormedIsIgnored_data()
    {
        QTest::addColumn<QByteArray>("bytes");
        QTest::newRow("empty") << QByteArray();
        QTest::newRow("not json") << QByteArray("clean");
        QTest::newRow("an unknown name") << QByteArray(R"({"version":1,"memory":"yes"})");
        QTest::newRow("a name with other letters") << QByteArray(R"({"version":1,"memory":"Clean"})");
        QTest::newRow("a boolean") << QByteArray(R"({"version":1,"memory":true})");
        QTest::newRow("another version") << QByteArray(R"({"version":3,"memory":"clean"})");
        QTest::newRow("an extra key") << QByteArray(R"({"version":1,"memory":"clean","keys":"abc"})");
        QTest::newRow("too large") << (QByteArray(R"({"version":1,"memory":"clean","x":")") + QByteArray(400, 'a') + "\"}");
    }
    void aMemoryThatIsNotWellFormedIsIgnored()
    {
        QFETCH(const QByteArray, bytes);
        QVERIFY(!parseLightingMemory(bytes).has_value());
    }

    // The decision: both are needed.
    void theLightingIsPutBackOnlyWhenAskedAndEndedProperlyWithItOn_data()
    {
        QTest::addColumn<int>("asked"); // -1 none, 0 false, 1 true
        QTest::addColumn<int>("memory"); // -1 none, else LightingMemory
        QTest::addColumn<bool>("expected");
        const int off = static_cast<int>(LightingMemory::Off);
        const int running = static_cast<int>(LightingMemory::Running);
        const int clean = static_cast<int>(LightingMemory::CleanExit);
        QTest::newRow("asked, clean exit") << 1 << clean << true;
        QTest::newRow("asked, crashed (still running)") << 1 << running << false;
        QTest::newRow("asked, was off") << 1 << off << false;
        QTest::newRow("asked, no memory") << 1 << -1 << false;
        QTest::newRow("not asked, clean exit") << 0 << clean << false;
        QTest::newRow("no choice, clean exit") << -1 << clean << false; // off unless chosen
        QTest::newRow("nothing at all") << -1 << -1 << false;
    }
    void theLightingIsPutBackOnlyWhenAskedAndEndedProperlyWithItOn()
    {
        QFETCH(const int, asked);
        QFETCH(const int, memory);
        QFETCH(const bool, expected);
        const std::optional<bool> choice = asked < 0 ? std::nullopt : std::optional<bool>(asked == 1);
        const std::optional<LightingMemory> remembered = memory < 0 ? std::nullopt : std::optional<LightingMemory>(static_cast<LightingMemory>(memory));
        QCOMPARE(shouldRestoreLighting(choice, remembered), expected);
    }

    // At the start: the memory is read and put back to off at once.
    void aCleanExitThatWasAskedForIsRestoredAndTheMemoryIsOffRightAfter()
    {
        const QTemporaryDir dir;
        const QString restore = dir.filePath("lighting-restore.json");
        const QString memory = dir.filePath("lighting-memory.json");
        QVERIFY(writeLightingRestore(restore, true));
        QVERIFY(writeLightingMemory(memory, LightingMemory::CleanExit));
        QVERIFY(consumeLightingRestore(restore, memory));
        const auto after = readLightingMemory(memory);
        QVERIFY(after.has_value());
        QVERIFY(after.value_or(LightingMemory::CleanExit) == LightingMemory::Off); // a start that goes wrong is not repeated
        QVERIFY(!consumeLightingRestore(restore, memory)); // the same clean exit does not count twice
    }

    void aCrashIsNeverRestored()
    {
        const QTemporaryDir dir;
        const QString restore = dir.filePath("lighting-restore.json");
        const QString memory = dir.filePath("lighting-memory.json");
        QVERIFY(writeLightingRestore(restore, true));
        QVERIFY(writeLightingMemory(memory, LightingMemory::Running)); // what a crash leaves
        QVERIFY(!consumeLightingRestore(restore, memory));
    }

    void withoutTheChoiceOfThePersonNothingIsRestored()
    {
        const QTemporaryDir dir;
        const QString memory = dir.filePath("lighting-memory.json");
        QVERIFY(writeLightingMemory(memory, LightingMemory::CleanExit));
        QVERIFY(!consumeLightingRestore(dir.filePath("lighting-restore.json"), memory)); // no file
        QVERIFY(writeLightingMemory(memory, LightingMemory::CleanExit));
        QVERIFY(writeRaw(dir.filePath("lighting-restore.json"), R"({"version":1,"restore":false})"));
        QVERIFY(!consumeLightingRestore(dir.filePath("lighting-restore.json"), memory));
    }

    void damagedFilesRestoreNothing()
    {
        const QTemporaryDir dir;
        const QString restore = dir.filePath("lighting-restore.json");
        const QString memory = dir.filePath("lighting-memory.json");
        QVERIFY(writeRaw(restore, "{"));
        QVERIFY(writeLightingMemory(memory, LightingMemory::CleanExit));
        QVERIFY(!consumeLightingRestore(restore, memory));
        QVERIFY(writeLightingRestore(restore, true));
        QVERIFY(writeRaw(memory, "damaged"));
        QVERIFY(!consumeLightingRestore(restore, memory));
    }

    void ifTheMemoryCannotBeWrittenNothingIsRestored()
    {
        const QTemporaryDir dir;
        const QString restore = dir.filePath("lighting-restore.json");
        QVERIFY(writeLightingRestore(restore, true));
        // A folder where the file should be: it can be neither read as a memory nor replaced; a crash could not be told from a clean exit.
        QVERIFY(QDir().mkpath(dir.filePath("lighting-memory.json/inside")));
        QVERIFY(!consumeLightingRestore(restore, dir.filePath("lighting-memory.json")));
    }

    void theFilesHoldOnlyAnOnOrOffAndNothingAboutKeys()
    {
        // Only "on" or "off": no key, no sequence, no time of day.
        const QByteArray memory = encodeLightingMemory(LightingMemory::CleanExit);
        QVERIFY(memory.size() < 64);
        QVERIFY(!memory.contains("key"));
        QVERIFY(!encodeLightingRestore(true).contains("key"));
    }
};

QTEST_APPLESS_MAIN(TestLightingRestore)
#include "test_lightingrestore.moc"
