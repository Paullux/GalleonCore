// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

// "Lights off until morning" for the module and the rings, with a fake module: what is sent when the night starts and ends, the end time, and the
// file that carries a night over a restart of galleond. Never opens real hardware.
#include "FakeTransport.h"
#include "NightController.h"
#include "NightMode.h"

#include <galleon/StreamDeck.h>

#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <memory>

using namespace galleond;

namespace {

// The brightness reports sent so far (report 3, 0x08, percent), in order.
std::vector<int> brightnesses(const FakeTransport &transport)
{
    std::vector<int> found;
    for (const auto &report : transport.featureReports) {
        if (report.size() > 2 && report[0] == 0x03 && report[1] == 0x08)
            found.push_back(report[2]);
    }
    return found;
}

int screenReports(const FakeTransport &transport)
{
    int count = 0;
    for (const auto &r : transport.writes)
        count += r.size() > 3 && r[0] == 0x02 && r[1] == 0x0c ? 1 : 0;
    return count;
}

QSet<int> keysDrawn(const FakeTransport &transport)
{
    QSet<int> keys;
    for (const auto &r : transport.writes) {
        if (r.size() > 3 && r[0] == 0x02 && r[1] == 0x07)
            keys.insert(r[2]);
    }
    return keys;
}

// A ring report: report 3, 0x24, the LED, then the colour (see protocol::dialRingReports).
int ringReports(const FakeTransport &transport)
{
    int count = 0;
    for (const auto &r : transport.featureReports)
        count += r.size() > 2 && r[0] == 0x03 && r[1] == 0x24 ? 1 : 0;
    return count;
}

struct Rig
{
    QTemporaryDir dir;
    FakeTransport *transport = nullptr;
    std::unique_ptr<galleon::StreamDeck> deck;
    QDateTime now{QDate(2026, 11, 22), QTime(23, 0)}; // local time, far from a change of summer time
    std::unique_ptr<NightController> night;

    explicit Rig(bool openDeck = true)
    {
        auto fake = std::make_unique<FakeTransport>();
        transport = fake.get();
        deck = std::make_unique<galleon::StreamDeck>(std::move(fake));
        if (openDeck)
            QVERIFY(deck->open());
        night = make();
    }
    [[nodiscard]] QString path() const { return dir.filePath("night-mode.json"); }
    [[nodiscard]] std::unique_ptr<NightController> make() { return std::make_unique<NightController>(deck.get(), path(), [this] { return now; }); }
    [[nodiscard]] qint64 seconds() const { return now.toSecsSinceEpoch(); }
};

} // namespace

class TestNightController : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void startingTheNightMakesTheModuleDarkAndSaysSo()
    {
        Rig rig;
        QSignalSpy active(rig.night.get(), &NightController::activeChanged);
        QVERIFY(rig.night->start(7, 0));
        QVERIFY(rig.night->isActive());
        QCOMPARE(active.count(), 1);
        QVERIFY(active.first().first().toBool());
        const auto levels = brightnesses(*rig.transport);
        QVERIFY(!levels.empty());
        QCOMPARE(levels.back(), 0); // the brightness is at 0
        QVERIFY(screenReports(*rig.transport) > 0); // a black screen
        QCOMPARE(keysDrawn(*rig.transport).size(), 12); // and 12 black keys
        QVERIFY(ringReports(*rig.transport) > 0); // and the rings out
    }

    void theEndIsTheNextTimeOfThatNameAndIsKept()
    {
        Rig rig;
        QVERIFY(rig.night->start(7, 0));
        const QDateTime expected(QDate(2026, 11, 23), QTime(7, 0));
        QCOMPARE(rig.night->until(), expected.toSecsSinceEpoch());
        QCOMPARE(readNightUntil(rig.path(), rig.seconds()).value_or(0), expected.toSecsSinceEpoch()); // in the file, for a restart
    }

    void aTimeOutOfRangeIsRefusedAndNothingHappens()
    {
        Rig rig;
        const QSignalSpy active(rig.night.get(), &NightController::activeChanged);
        QVERIFY(!rig.night->start(24, 0));
        QVERIFY(!rig.night->start(7, 60));
        QVERIFY(!rig.night->start(-1, 0));
        QVERIFY(!rig.night->isActive());
        QCOMPARE(active.count(), 0);
        QVERIFY(rig.transport->featureReports.empty() || brightnesses(*rig.transport).empty());
        QCOMPARE(screenReports(*rig.transport), 0);
        QVERIFY(!QFile::exists(rig.path()));
    }

    void endingTheNightGivesTheBrightnessBackRemovesTheFileAndSaysSo()
    {
        Rig rig;
        QVERIFY(rig.night->start(7, 0));
        const QSignalSpy active(rig.night.get(), &NightController::activeChanged);
        rig.night->end();
        QVERIFY(!rig.night->isActive());
        QCOMPARE(rig.night->until(), qint64{0});
        QCOMPARE(active.count(), 1);
        QVERIFY(!active.first().first().toBool());
        QCOMPARE(brightnesses(*rig.transport).back(), NightDayBrightness);
        QVERIFY(!QFile::exists(rig.path()));
        rig.night->end(); // already over: nothing more
        QCOMPARE(active.count(), 1);
    }

    void theNightEndsByItselfWhenItsTimeComes()
    {
        Rig rig;
        rig.night->setTickMs(20);
        QVERIFY(rig.night->start(7, 0)); // the next morning
        const QSignalSpy active(rig.night.get(), &NightController::activeChanged);
        QTest::qWait(100);
        QVERIFY(rig.night->isActive()); // the night is long
        rig.now = QDateTime(QDate(2026, 11, 23), QTime(7, 0, 1)); // the clock reaches the morning
        QTRY_VERIFY_WITH_TIMEOUT(!rig.night->isActive(), 3000);
        QCOMPARE(active.count(), 1);
        QCOMPARE(brightnesses(*rig.transport).back(), NightDayBrightness);
        QVERIFY(!QFile::exists(rig.path()));
    }

    void aClockThatJumpsPastTheEndEndsTheNightAtTheNextLook()
    {
        // The computer slept through the morning: the next look at the clock ends it.
        Rig rig;
        rig.night->setTickMs(20);
        QVERIFY(rig.night->start(7, 0));
        rig.now = QDateTime(QDate(2026, 11, 24), QTime(15, 0));
        QTRY_VERIFY_WITH_TIMEOUT(!rig.night->isActive(), 3000);
    }

    void startingAgainDuringTheNightOnlyMovesTheEnd()
    {
        Rig rig;
        QVERIFY(rig.night->start(7, 0));
        const QSignalSpy active(rig.night.get(), &NightController::activeChanged);
        QVERIFY(rig.night->start(9, 30));
        QCOMPARE(active.count(), 0); // still the same night
        QCOMPARE(rig.night->until(), QDateTime(QDate(2026, 11, 23), QTime(9, 30)).toSecsSinceEpoch());
        QCOMPARE(readNightUntil(rig.path(), rig.seconds()).value_or(0), rig.night->until());
    }

    void aNightThatWasGoingOnIsTakenUpAfterARestart()
    {
        Rig rig;
        QVERIFY(rig.night->start(7, 0));
        const qint64 until = rig.night->until();
        rig.night.reset(); // galleond stops without ending the night (a restart, a crash)
        auto again = rig.make();
        const QSignalSpy active(again.get(), &NightController::activeChanged);
        QVERIFY(again->resume());
        QVERIFY(again->isActive());
        QCOMPARE(again->until(), until); // not early
        QCOMPARE(active.count(), 1);
        QCOMPARE(brightnesses(*rig.transport).back(), 0);
    }

    void aNightThatEndedWhileGalleondWasStoppedIsNotTakenUpAndItsFileIsRemoved()
    {
        Rig rig;
        QVERIFY(rig.night->start(7, 0));
        rig.night.reset();
        rig.now = QDateTime(QDate(2026, 11, 23), QTime(8, 0)); // morning has come
        auto again = rig.make();
        QVERIFY(!again->resume());
        QVERIFY(!again->isActive());
        QVERIFY(!QFile::exists(rig.path())); // so that the settings window shows no night
    }

    void aDamagedOrFarAwayFileIsNeverTakenUp()
    {
        Rig rig;
        QVERIFY(writeNightUntil(rig.path(), rig.seconds() + 3 * MaxNightSeconds)); // someone wrote an end days away
        auto again = rig.make();
        QVERIFY(!again->resume());
        QVERIFY(!again->isActive());
        QVERIFY(!QFile::exists(rig.path()));
        QFile file(rig.path());
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("damaged");
        file.close();
        QVERIFY(!again->resume());
        QVERIFY(!QFile::exists(rig.path()));
    }

    void withoutAFileThereIsNothingToTakeUp()
    {
        Rig rig;
        const QSignalSpy active(rig.night.get(), &NightController::activeChanged);
        QVERIFY(!rig.night->resume());
        QCOMPARE(active.count(), 0);
        QCOMPARE(screenReports(*rig.transport), 0); // the module is left alone
    }

    void aModuleThatComesBackGetsTheDarkAgainWhileTheNightLasts()
    {
        Rig rig;
        QVERIFY(rig.night->start(7, 0));
        const std::size_t before = rig.transport->writes.size();
        rig.night->reapply();
        QVERIFY(rig.transport->writes.size() > before); // black pictures again
        QCOMPARE(brightnesses(*rig.transport).back(), 0);
    }

    void aModuleThatComesBackAfterTheNightGetsNothing()
    {
        Rig rig;
        const std::size_t before = rig.transport->writes.size();
        rig.night->reapply();
        QCOMPARE(rig.transport->writes.size(), before);
        QVERIFY(brightnesses(*rig.transport).empty());
    }

    void aModuleThatIsNotThereYetStillStartsTheNightAndGetsTheDarkWhenItComes()
    {
        Rig rig(false); // not open
        QVERIFY(rig.night->start(7, 0));
        QVERIFY(rig.night->isActive());
        QCOMPARE(screenReports(*rig.transport), 0);
        QVERIFY(rig.deck->open());
        rig.night->reapply();
        QVERIFY(screenReports(*rig.transport) > 0);
    }

    void aFileThatCannotBeWrittenDoesNotStopTheNight()
    {
        Rig rig;
        NightController night(rig.deck.get(), QStringLiteral("/proc/no-such-place/night-mode.json"), [&rig] { return rig.now; });
        QVERIFY(night.start(7, 0)); // the person asked for the dark: it starts, and will not survive a restart
        QVERIFY(night.isActive());
    }

    void withoutAPathNothingIsKept()
    {
        Rig rig;
        NightController night(rig.deck.get(), QString(), [&rig] { return rig.now; });
        QVERIFY(night.start(7, 0));
        QVERIFY(!night.resume());
    }
};

QTEST_MAIN(TestNightController)
#include "test_nightcontroller.moc"
