// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

// "Lights off until morning": the end time is a pure function, and the file that keeps the night over a restart is untrusted input: a damaged,
// expired or absurd file must never keep the module dark.
#include "NightMode.h"

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

// Local time, so that the tests do not depend on a time zone being available; a date far from any change of summer time.
QDateTime at(int hour, int minute, int second = 0)
{
    return {QDate(2026, 11, 22), QTime(hour, minute, second)};
}

} // namespace

class TestNightMode : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void theNightEndsAtTheNextTimeOfThatNameTodayWhenItIsStillAhead()
    {
        QCOMPARE(nightEnd(at(23, 0), 23, 30), at(23, 30));
        QCOMPARE(nightEnd(at(1, 0), 7, 0), at(7, 0));
    }

    void theNightEndsTomorrowWhenThatTimeHasPassedToday()
    {
        QCOMPARE(nightEnd(at(23, 0), 7, 0), at(7, 0).addDays(1));
        QCOMPARE(nightEnd(at(7, 30), 7, 0), at(7, 0).addDays(1));
    }

    void anEndEqualToTheTimeNowIsAWholeDayAway()
    {
        QCOMPARE(nightEnd(at(7, 0), 7, 0), at(7, 0).addDays(1));
        QCOMPARE(nightEnd(at(7, 0, 30), 7, 0), at(7, 0).addDays(1)); // seconds past the minute: it has passed
    }

    void theEndIsNeverMoreThanADayAway()
    {
        for (int hour = 0; hour < 24; ++hour) {
            for (const int minute : {0, 30, 59}) {
                const QDateTime now = at(12, 15);
                const QDateTime end = nightEnd(now, hour, minute);
                QVERIFY(end > now);
                QVERIFY(now.secsTo(end) <= MaxNightSeconds);
            }
        }
    }

    void anHourOrAMinuteOutOfRangeGivesNoEnd()
    {
        QVERIFY(!nightEnd(at(12, 0), 24, 0).isValid());
        QVERIFY(!nightEnd(at(12, 0), -1, 0).isValid());
        QVERIFY(!nightEnd(at(12, 0), 7, 60).isValid());
        QVERIFY(!nightEnd(at(12, 0), 7, -1).isValid());
        QVERIFY(!nightEnd(QDateTime(), 7, 0).isValid());
    }

    void theEndIsReadBackAsItWasWritten()
    {
        const qint64 now = 1800000000;
        const auto read = parseNightUntil(encodeNightUntil(now + 3600), now);
        QVERIFY(read.has_value());
        QCOMPARE(read.value_or(0), now + 3600);
    }

    void aFileThatIsNotWellFormedIsIgnored_data()
    {
        QTest::addColumn<QByteArray>("bytes");
        QTest::newRow("empty") << QByteArray();
        QTest::newRow("not json") << QByteArray("tomorrow");
        QTest::newRow("an array") << QByteArray("[1800003600]");
        QTest::newRow("a string") << QByteArray(R"({"version":1,"until":"1800003600"})");
        QTest::newRow("another version") << QByteArray(R"({"version":2,"until":1800003600})");
        QTest::newRow("a missing key") << QByteArray(R"({"version":1})");
        QTest::newRow("an extra key") << QByteArray(R"({"version":1,"until":1800003600,"keys":"abc"})");
        QTest::newRow("a fraction") << QByteArray(R"({"version":1,"until":1800003600.5})");
        QTest::newRow("already past") << QByteArray(R"({"version":1,"until":1799999999})");
        QTest::newRow("exactly now") << QByteArray(R"({"version":1,"until":1800000000})");
        QTest::newRow("more than a day away") << QByteArray(R"({"version":1,"until":1800086401})");
        QTest::newRow("absurdly far") << QByteArray(R"({"version":1,"until":9999999999999})");
        QTest::newRow("a huge number") << QByteArray(R"({"version":1,"until":1e300})");
        QTest::newRow("negative") << QByteArray(R"({"version":1,"until":-5})");
        QTest::newRow("too large") << (QByteArray(R"({"version":1,"until":1800003600,"x":")") + QByteArray(400, 'a') + "\"}");
    }
    void aFileThatIsNotWellFormedIsIgnored()
    {
        QFETCH(const QByteArray, bytes);
        QVERIFY(!parseNightUntil(bytes, 1800000000).has_value());
    }

    void anEndExactlyADayAwayIsStillAccepted()
    {
        QVERIFY(parseNightUntil(R"({"version":1,"until":1800086400})", 1800000000).has_value());
    }

    void theFileIsWrittenAtomicallyReadBackAndRemoved()
    {
        const QTemporaryDir dir;
        const QString path = dir.filePath("night-mode.json");
        const qint64 now = 1800000000;
        QVERIFY(writeNightUntil(path, now + 600));
        QCOMPARE(readNightUntil(path, now).value_or(0), now + 600);
        QVERIFY(removeNightFile(path));
        QVERIFY(!QFile::exists(path));
        QVERIFY(removeNightFile(path)); // already gone is fine
        QVERIFY(!readNightUntil(path, now).has_value());
    }

    void anExpiredFileOnDiskIsNotUsed()
    {
        const QTemporaryDir dir;
        const QString path = dir.filePath("night-mode.json");
        QVERIFY(writeNightUntil(path, 1800000600));
        QVERIFY(!readNightUntil(path, 1800000700).has_value()); // the night ended 100 s ago
    }

    void aDamagedFileOnDiskIsNotUsed()
    {
        const QTemporaryDir dir;
        const QString path = dir.filePath("night-mode.json");
        QVERIFY(writeRaw(path, "{"));
        QVERIFY(!readNightUntil(path, 1800000000).has_value());
    }

    void theFileHoldsOnlyAnEndAndNothingAboutKeys()
    {
        const QByteArray bytes = encodeNightUntil(1800003600);
        QVERIFY(bytes.size() < 64);
        QVERIFY(!bytes.contains("key"));
    }
};

QTEST_APPLESS_MAIN(TestNightMode)
#include "test_nightmode.moc"
