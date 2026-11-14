// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

// galleond's live lighting component with a fake keyboard (FakeLiveKeyboard.h), a fake watchdog and a fake sleep. The
// clock is the test's: moving it forward is how the 3 s of listening pass without waiting. Never opens real hardware,
// never starts a process, and proves nothing about a real keyboard.

#include "FakeLiveKeyboard.h"
#include "LightingController.h"

#include <QSignalSpy>
#include <QTest>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

using namespace galleond;
using fakelive::FakeDevice;
using galleon::keyboard::LiveClock;

namespace {

// What happened, in order, from several threads.
class Journal
{
public:
    void add(const std::string &entry)
    {
        const std::scoped_lock lock(m_mutex);
        m_entries.push_back(entry);
    }
    [[nodiscard]] std::vector<std::string> entries() const
    {
        const std::scoped_lock lock(m_mutex);
        return m_entries;
    }
    [[nodiscard]] int count(const std::string &entry) const
    {
        const auto all = entries();
        return static_cast<int>(std::count(all.begin(), all.end(), entry));
    }
    // The position of the first such entry, or -1.
    [[nodiscard]] int first(const std::string &entry) const
    {
        const auto all = entries();
        const auto found = std::find(all.begin(), all.end(), entry);
        return found == all.end() ? -1 : static_cast<int>(found - all.begin());
    }

private:
    mutable std::mutex m_mutex;
    std::vector<std::string> m_entries;
};

class FakeWatchdog final : public WatchdogGuard
{
    Q_OBJECT

public:
    explicit FakeWatchdog(Journal &journal) : m_journal(journal) {}
    bool start(qint64 pid, QString *why) override
    {
        m_journal.add("watchdog.start");
        startedFor = pid;
        if (!works)
            *why = QStringLiteral("no watchdog here");
        return works;
    }
    void stop() override { m_journal.add("watchdog.stop"); }
    void die() { Q_EMIT died(); }

    bool works = true;
    qint64 startedFor = 0;

private:
    Journal &m_journal;
};

class FakePower final : public PowerEvents
{
    Q_OBJECT

public:
    explicit FakePower(Journal &journal) : m_journal(journal) {}
    bool available(QString *why) const override
    {
        if (!isAvailable)
            *why = QStringLiteral("no sleep events here");
        return isAvailable;
    }
    void holdSleep() override { m_journal.add("power.hold"); }
    void allowSleep() override { m_journal.add("power.allow"); }
    void sleep() { Q_EMIT aboutToSleep(); }
    void wake() { Q_EMIT resumed(); }

    bool isAvailable = true;

private:
    Journal &m_journal;
};

// Forwards to a device the test keeps, so the test sees what the worker thread did to it.
class SharedTransport final : public galleon::Transport
{
public:
    explicit SharedTransport(FakeDevice &device) : m_device(device) {}
    bool open() override { return m_device.open(); }
    void close() override { m_device.close(); }
    [[nodiscard]] bool isOpen() const override { return m_device.isOpen(); }
    [[nodiscard]] QString serialNumber() const override { return m_device.serialNumber(); }
    bool sendFeatureReport(std::span<const std::uint8_t> report) override { return m_device.sendFeatureReport(report); }
    bool write(std::span<const std::uint8_t> report) override { return m_device.write(report); }
    int read(std::span<std::uint8_t> buffer) override { return m_device.read(buffer); }

private:
    FakeDevice &m_device;
};

struct Rig
{
    Journal journal;
    FakeDevice device;
    FakeWatchdog watchdog{journal};
    FakePower power{journal};
    std::atomic<long long> clockMs{0};
    const LiveClock::time_point origin = LiveClock::now();
    bool keyboardPresent = true;
    std::unique_ptr<LightingController> controller;

    Rig()
    {
        controller = std::make_unique<LightingController>(
            [this]() -> std::unique_ptr<galleon::Transport> {
                journal.add("opener");
                if (!keyboardPresent)
                    return nullptr;
                return std::make_unique<SharedTransport>(device);
            },
            watchdog, power, [this] { return origin + std::chrono::milliseconds(clockMs.load()); });
    }
    void advance(long long ms) { clockMs += ms; }
    // What the keyboard holds, read while the worker thread may be using it.
    [[nodiscard]] int mode()
    {
        const std::scoped_lock lock(device.mutex);
        return device.mode;
    }
    [[nodiscard]] int frames()
    {
        const std::scoped_lock lock(device.mutex);
        return device.framesComplete;
    }
    [[nodiscard]] std::vector<std::uint8_t> frame()
    {
        const std::scoped_lock lock(device.mutex);
        return device.lastFrame;
    }
    [[nodiscard]] std::size_t reports()
    {
        const std::scoped_lock lock(device.mutex);
        return device.sent.size();
    }
    // Enable, let the 3 s of listening pass, and wait for the keys to be taken.
    void hold()
    {
        QVERIFY(controller->Enable(true).isEmpty());
        QTRY_VERIFY_WITH_TIMEOUT(controller->state() == QStringLiteral("listening"), 3000);
        advance(4000);
        QTRY_VERIFY_WITH_TIMEOUT(controller->state() == QStringLiteral("holding"), 3000);
    }
};

// The colour of one LED in the last frame the keyboard received.
std::vector<std::uint8_t> ledColour(const std::vector<std::uint8_t> &frame, int led)
{
    if (frame.size() < galleon::keyboard::LiveFrameSize)
        return {};
    const std::size_t at = 2 + 3 * static_cast<std::size_t>(led);
    return {frame[at], frame[at + 1], frame[at + 2]};
}

} // namespace

class TestLightingController : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void aReleaseThatCannotBeConfirmedLeavesTheWatchdogRunning()
    {
        Rig rig;
        rig.hold();
        QVERIFY(rig.controller->active());
        {
            const std::scoped_lock lock(rig.device.mutex);
            rig.device.disconnected = true; // the keyboard stops answering: the release cannot be confirmed
        }
        QVERIFY(rig.controller->Enable(false).isEmpty());
        QVERIFY(!rig.controller->active());
        QCOMPARE(rig.controller->state(), QStringLiteral("off"));
        const auto entries = rig.journal.entries();
        QVERIFY2(std::find(entries.begin(), entries.end(), "watchdog.stop") == entries.end(),
                 "the watchdog must keep watching a keyboard that may still be in host mode");
    }

    void aKeyboardThatStopsAnsweringOnItsOwnLeavesTheWatchdogRunningToo()
    {
        Rig rig;
        rig.hold();
        {
            const std::scoped_lock lock(rig.device.mutex);
            rig.device.disconnected = true;
        }
        rig.advance(2000); // the worker notices by itself, before anyone asks to switch off
        QTRY_VERIFY_WITH_TIMEOUT(!rig.controller->active(), 5000);
        const auto entries = rig.journal.entries();
        QVERIFY2(std::find(entries.begin(), entries.end(), "watchdog.stop") == entries.end(),
                 "a read error is not proof that the keyboard was unplugged: its mode was never seen back at 1");
    }

    void aConfirmedReleaseStopsTheWatchdog()
    {
        Rig rig;
        rig.hold();
        QVERIFY(rig.controller->Enable(false).isEmpty());
        const auto entries = rig.journal.entries();
        QVERIFY(std::find(entries.begin(), entries.end(), "watchdog.stop") != entries.end());
        QCOMPARE(rig.mode(), 1);
    }

    void nothingHappensUntilItIsAskedFor()
    {
        Rig rig;
        QTest::qWait(150);
        QVERIFY(rig.journal.entries().empty());
        QCOMPARE(rig.controller->state(), QStringLiteral("off"));
        QVERIFY(!rig.controller->active());
        QCOMPARE(rig.reports(), std::size_t{0});
    }

    void withoutSleepEventsItIsRefusedAndNothingIsStartedOrOpened()
    {
        Rig rig;
        rig.power.isAvailable = false;
        const QSignalSpy active(rig.controller.get(), &LightingController::activeChanged);
        const QString answer = rig.controller->Enable(true);
        QVERIFY(!answer.isEmpty());
        QCOMPARE(rig.journal.count("watchdog.start"), 0);
        QCOMPARE(rig.journal.count("opener"), 0);
        QCOMPARE(rig.journal.count("power.hold"), 0);
        QCOMPARE(active.size(), 0);
        QVERIFY(!rig.controller->active());
    }

    void withoutAWorkingWatchdogItIsRefusedAndTheKeyboardIsNeverOpened()
    {
        Rig rig;
        rig.watchdog.works = false;
        const QString answer = rig.controller->Enable(true);
        QCOMPARE(answer, QStringLiteral("no watchdog here"));
        QCOMPARE(rig.journal.count("watchdog.start"), 1);
        QCOMPARE(rig.journal.count("opener"), 0); // the keyboard was not touched
        QCOMPARE(rig.journal.count("power.hold"), 0);
        QCOMPARE(rig.reports(), std::size_t{0});
        QVERIFY(!rig.controller->active());
    }

    void theWatchdogIsStartedForThisProcessBeforeTheKeyboardIsOpened()
    {
        Rig rig;
        QVERIFY(rig.controller->Enable(true).isEmpty());
        QTRY_VERIFY_WITH_TIMEOUT(rig.journal.count("opener") == 1, 3000);
        QCOMPARE(rig.watchdog.startedFor, QCoreApplication::applicationPid());
        QVERIFY(rig.journal.first("watchdog.start") >= 0);
        QVERIFY(rig.journal.first("watchdog.start") < rig.journal.first("opener"));
        QVERIFY(rig.journal.first("power.hold") < rig.journal.first("opener")); // the system waits for us before we start
    }

    void nothingIsSentWhileItListensAndTheKeysAreTakenAfterThreeSeconds()
    {
        Rig rig;
        QVERIFY(rig.controller->Enable(true).isEmpty());
        QTRY_VERIFY_WITH_TIMEOUT(rig.controller->state() == QStringLiteral("listening"), 3000);
        QTest::qWait(200);
        QCOMPARE(rig.reports(), std::size_t{0});
        rig.advance(2900);
        QTest::qWait(200);
        QCOMPARE(rig.reports(), std::size_t{0}); // 2.9 s: still listening
        rig.advance(200);
        QTRY_VERIFY_WITH_TIMEOUT(rig.controller->state() == QStringLiteral("holding"), 3000);
        QCOMPARE(rig.mode(), 2);
    }

    void aColourIsShownOnEveryKey()
    {
        Rig rig;
        rig.hold();
        QCOMPARE(rig.controller->state(), QStringLiteral("holding"));
        QVERIFY(rig.controller->SetColor(10, 20, 30));
        QTRY_VERIFY_WITH_TIMEOUT((ledColour(rig.frame(), 0x3a) == std::vector<std::uint8_t>{10, 20, 30}), 3000);
        QCOMPARE(ledColour(rig.frame(), 0), (std::vector<std::uint8_t>{10, 20, 30}));
        QCOMPARE(ledColour(rig.frame(), 145), (std::vector<std::uint8_t>{10, 20, 30}));
    }

    void aColourOutOfRangeIsRefusedAndTheLastOneStays()
    {
        Rig rig;
        rig.hold();
        QCOMPARE(rig.controller->state(), QStringLiteral("holding"));
        QVERIFY(!rig.controller->SetColor(256, 0, 0));
        QVERIFY(!rig.controller->SetColor(0, -1, 0));
        QVERIFY(rig.controller->SetColor(1, 2, 3));
    }

    void switchingItOffGivesTheKeysBackAndStopsTheWatchdog()
    {
        Rig rig;
        rig.hold();
        QCOMPARE(rig.controller->state(), QStringLiteral("holding"));
        const QSignalSpy active(rig.controller.get(), &LightingController::activeChanged);
        QVERIFY(rig.controller->Enable(false).isEmpty());
        QCOMPARE(rig.mode(), 1); // back to its own mode before Enable returned
        QCOMPARE(rig.journal.count("watchdog.stop"), 1);
        QVERIFY(rig.journal.count("power.allow") >= 1);
        QCOMPARE(rig.controller->state(), QStringLiteral("off"));
        QVERIFY(!rig.controller->active());
        QCOMPARE(active.size(), 1);
        QVERIFY(!active.constFirst().constFirst().toBool());
    }

    void switchingItOnTwiceIsRefused()
    {
        Rig rig;
        rig.hold();
        QCOMPARE(rig.controller->state(), QStringLiteral("holding"));
        QVERIFY(!rig.controller->Enable(true).isEmpty());
        QCOMPARE(rig.journal.count("watchdog.start"), 1);
    }

    void switchingOffWhenItIsOffIsAnAnswerNotAnError()
    {
        Rig rig;
        QVERIFY(rig.controller->Enable(false).isEmpty());
        QVERIFY(rig.journal.entries().empty());
    }

    void destroyingItGivesTheKeysBack()
    {
        Rig rig;
        rig.hold();
        QCOMPARE(rig.controller->state(), QStringLiteral("holding"));
        QCOMPARE(rig.mode(), 2);
        rig.controller.reset(); // galleond leaves
        QCOMPARE(rig.mode(), 1);
        QCOMPARE(rig.journal.count("watchdog.stop"), 1);
    }

    void aWatchdogThatDiesWhileItIsOnStopsItAndGivesTheKeysBack()
    {
        Rig rig;
        rig.hold();
        QCOMPARE(rig.controller->state(), QStringLiteral("holding"));
        const QSignalSpy states(rig.controller.get(), &LightingController::stateChanged);
        rig.watchdog.die();
        QCOMPARE(rig.controller->state(), QStringLiteral("off"));
        QVERIFY(!rig.controller->active());
        QCOMPARE(rig.mode(), 1);
        QVERIFY(states.constLast().at(1).toString().contains(QStringLiteral("watchdog")));
    }

    void beforeTheMachineSleepsTheKeysAreGivenBackAndAfterWakingTheyAreTakenAgain()
    {
        Rig rig;
        rig.hold();
        QCOMPARE(rig.controller->state(), QStringLiteral("holding"));
        const int allowsBefore = rig.journal.count("power.allow");
        rig.power.sleep();
        QCOMPARE(rig.mode(), 1); // given back before the system was let go to sleep
        QCOMPARE(rig.controller->state(), QStringLiteral("sleeping"));
        QCOMPARE(rig.journal.count("power.allow"), allowsBefore + 1);
        QVERIFY(rig.controller->active()); // still wanted: it will come back
        QCOMPARE(rig.journal.count("watchdog.stop"), 0); // the watchdog stays: a crash while asleep is covered too

        rig.power.wake();
        QTRY_VERIFY_WITH_TIMEOUT(rig.controller->state() == QStringLiteral("listening"), 3000); // 3 s of listening again
        rig.advance(4000);
        QTRY_VERIFY_WITH_TIMEOUT(rig.controller->state() == QStringLiteral("holding"), 3000);
        QCOMPARE(rig.mode(), 2);
        QCOMPARE(rig.journal.count("watchdog.start"), 1);
    }

    void aKeyboardThatCannotBeOpenedStopsEverythingAndSaysWhy()
    {
        Rig rig;
        rig.keyboardPresent = false;
        const QSignalSpy states(rig.controller.get(), &LightingController::stateChanged);
        QVERIFY(rig.controller->Enable(true).isEmpty()); // the keyboard is only opened on the worker thread
        QTRY_VERIFY_WITH_TIMEOUT(!rig.controller->active(), 3000);
        QCOMPARE(rig.controller->state(), QStringLiteral("off"));
        QVERIFY(!states.constLast().at(1).toString().isEmpty());
        QCOMPARE(rig.journal.count("watchdog.stop"), 1);
        QCOMPARE(rig.reports(), std::size_t{0});
    }

    void anotherHostMakesItYieldAndNothingMoreIsSent()
    {
        Rig rig;
        rig.hold();
        QCOMPARE(rig.controller->state(), QStringLiteral("holding"));
        rig.device.otherHostSpeaks();
        QTRY_VERIFY_WITH_TIMEOUT(rig.controller->state() == QStringLiteral("yielded"), 3000);
        const std::size_t sent = rig.reports();
        QVERIFY(rig.controller->SetColor(200, 0, 0));
        QTest::qWait(300);
        QCOMPARE(rig.reports(), sent);
        QVERIFY(rig.controller->active()); // still on: it resumes after a minute of quiet
    }

    void twoFailedFramesInARowStopItAndLeaveItOffUntilAskedAgain()
    {
        Rig rig;
        rig.hold();
        QCOMPARE(rig.controller->state(), QStringLiteral("holding"));
        QTest::qWait(100);
        {
            const std::scoped_lock lock(rig.device.mutex);
            rig.device.silentFrames = true;
        }
        QVERIFY(rig.controller->SetColor(5, 5, 5));
        // The clock moves on between the attempts, in small steps: the frames are paced (a frozen clock would never allow a
        // second one), and the poll of the mode, a success that resets the count, is due only a second after the keys
        // were taken, which the two attempts do not reach.
        for (int i = 0; i < 100 && rig.controller->active(); ++i) {
            rig.advance(60);
            QTest::qWait(100);
        }
        QVERIFY(!rig.controller->active());
        QCOMPARE(rig.controller->state(), QStringLiteral("off"));
        QCOMPARE(rig.mode(), 1); // the keys were given back as it stopped
        QCOMPARE(rig.journal.count("watchdog.stop"), 1);
    }
};

QTEST_GUILESS_MAIN(TestLightingController)
#include "test_lightingcontroller.moc"
