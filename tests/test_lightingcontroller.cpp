// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

// galleond's live lighting component with a fake keyboard (FakeLiveKeyboard.h), a fake watchdog and a fake sleep. The
// clock is the test's: moving it forward is how the 3 s of listening pass without waiting. Never opens real hardware,
// never starts a process, and proves nothing about a real keyboard.

#include "FakeLiveKeyboard.h"
#include "AppearanceFile.h"
#include "BrightnessCurve.h"
#include "LightingAppearanceApplier.h"
#include "LightingController.h"
#include "LightingLevelScheduler.h"
#include "LightingSkyDay.h"
#include "SkySun.h"

#include <QMetaMethod>
#include <QSignalSpy>
#include <QFile>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <QDateTime>
#include <QTimeZone>
#ifndef Q_OS_WIN
#include <QDBusMetaType>
#endif
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

// The clock of the sky for the tests: noon of the summer solstice in Paris (UTC+2).
QDateTime parisSolstice()
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    const QTimeZone summerTimeInParis = QTimeZone::fromSecondsAheadOfUtc(7200); // the form of Qt 6.5 and later
#else
    const QTimeZone summerTimeInParis(7200); // the form of Qt 6.4, which the CI builds with
#endif
    return {QDate(2026, 6, 21), QTime(12, 0), summerTimeInParis};
}

galleon::keyboard::Appearance skyAppearance(bool followSun, std::optional<galleon::keyboard::GeoPlace> place = std::nullopt)
{
    galleon::keyboard::Appearance appearance;
    appearance.base = galleon::keyboard::BaseType::Sky;
    appearance.skyAnimated = false; // the colours of the hour alone: the shimmer and the stars have their own tests
    appearance.followSun = followSun;
    appearance.skyPlace = place;
    return appearance;
}

void writeWidgetsPlace(const QString &path, double latitude, double longitude)
{
    QFile out(path);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return;
    out.write(QStringLiteral(R"({"version":1,"location":{"latitude":%1,"longitude":%2}})").arg(latitude, 0, 'f', 6).arg(longitude, 0, 'f', 6).toUtf8());
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

    void everyKeyIsDarkUntilAColourIsAskedFor()
    {
        Rig rig;
        rig.hold();
        QCOMPARE(rig.controller->state(), QStringLiteral("holding"));
        QTRY_VERIFY_WITH_TIMEOUT(rig.frames() >= 1, 3000);
        for (const int led : {0, 0x3a, 100, 145})
            QCOMPARE(ledColour(rig.frame(), led), (std::vector<std::uint8_t>{0, 0, 0}));
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

    void oneLedCanBeLitAloneForTheCalibrationOfTheKeyMap()
    {
        Rig rig;
        rig.hold();
        QCOMPARE(rig.controller->state(), QStringLiteral("holding"));
        QVERIFY(rig.controller->SetLed(0x3a, 1, 2, 3));
        QTRY_VERIFY_WITH_TIMEOUT((ledColour(rig.frame(), 0x3a) == std::vector<std::uint8_t>{1, 2, 3}), 3000);
        QCOMPARE(ledColour(rig.frame(), 0), (std::vector<std::uint8_t>{0, 0, 0}));
        QCOMPARE(ledColour(rig.frame(), 0x3b), (std::vector<std::uint8_t>{0, 0, 0}));
        QCOMPARE(ledColour(rig.frame(), 145), (std::vector<std::uint8_t>{0, 0, 0}));
    }

    void aColourAfterOneLedGoesBackToEveryKey()
    {
        Rig rig;
        rig.hold();
        QVERIFY(rig.controller->SetLed(5, 9, 9, 9));
        QTRY_VERIFY_WITH_TIMEOUT((ledColour(rig.frame(), 5) == std::vector<std::uint8_t>{9, 9, 9}), 3000);
        rig.advance(200); // the frames are paced on the test's clock: a second one needs time to have passed
        QVERIFY(rig.controller->SetColor(7, 8, 9));
        QTRY_VERIFY_WITH_TIMEOUT((ledColour(rig.frame(), 0x3a) == std::vector<std::uint8_t>{7, 8, 9}), 3000);
    }

    void aLedOrAColourOutOfRangeIsRefusedForOneLed()
    {
        Rig rig;
        rig.hold();
        QCOMPARE(rig.controller->state(), QStringLiteral("holding"));
        QVERIFY(!rig.controller->SetLed(146, 1, 1, 1));
        QVERIFY(!rig.controller->SetLed(-1, 1, 1, 1));
        QVERIFY(!rig.controller->SetLed(5, 256, 0, 0));
        QVERIFY(rig.controller->SetLed(145, 1, 1, 1));
    }

    void anAppearanceSetBeforeSwitchingOnIsWhatTheKeysShowWhenTheyAreTaken()
    {
        Rig rig;
        galleon::keyboard::Appearance appearance;
        appearance.colour = {10, 20, 30};
        QVERIFY(rig.controller->setAppearance(appearance)); // asked for while the lighting is off: kept for the start
        rig.hold();
        QCOMPARE(rig.controller->state(), QStringLiteral("holding"));
        QTRY_VERIFY_WITH_TIMEOUT((ledColour(rig.frame(), 58) == std::vector<std::uint8_t>{10, 20, 30}), 3000); // F1 lights something
        QCOMPARE(ledColour(rig.frame(), 0), (std::vector<std::uint8_t>{0, 0, 0}));                             // LED 0 lights nothing: no place, dark
    }

    void changingTheAppearanceWhileTheLightingIsOnChangesTheKeys()
    {
        Rig rig;
        rig.hold();
        galleon::keyboard::Appearance first;
        first.colour = {1, 2, 3};
        QVERIFY(rig.controller->setAppearance(first));
        QTRY_VERIFY_WITH_TIMEOUT((ledColour(rig.frame(), 58) == std::vector<std::uint8_t>{1, 2, 3}), 3000);
        rig.advance(200); // the frames are paced on the test's clock
        galleon::keyboard::Appearance second;
        second.colour = {7, 8, 9};
        second.retouches = {{4, {255, 255, 255}}};
        QVERIFY(rig.controller->setAppearance(second));
        QTRY_VERIFY_WITH_TIMEOUT((ledColour(rig.frame(), 58) == std::vector<std::uint8_t>{7, 8, 9}), 3000);
        QCOMPARE(ledColour(rig.frame(), 4), (std::vector<std::uint8_t>{255, 255, 255})); // the retouch is over the base
    }

    void aRainbowThatMovesChangesTheFramesWithTheClockAndAFixedOneDoesNot()
    {
        Rig rig;
        rig.hold();
        galleon::keyboard::Appearance moving;
        moving.base = galleon::keyboard::BaseType::Rainbow;
        moving.rainbowSpeed = 1000; // one turn a second
        QVERIFY(rig.controller->setAppearance(moving));
        QTRY_VERIFY_WITH_TIMEOUT(rig.frames() >= 1, 3000);
        const auto before = rig.frame();
        for (int i = 0; i < 6; ++i) { // 6 steps of 60 ms: a third of a turn
            rig.advance(60);
            QTest::qWait(120);
        }
        QVERIFY(rig.frame() != before);
        // A fixed rainbow sends nothing more once it is shown.
        rig.advance(200);
        galleon::keyboard::Appearance fixed;
        fixed.base = galleon::keyboard::BaseType::Rainbow;
        QVERIFY(rig.controller->setAppearance(fixed));
        QTest::qWait(200);
        const int framesNow = rig.frames();
        for (int i = 0; i < 5; ++i) {
            rig.advance(100);
            QTest::qWait(120);
        }
        QCOMPARE(rig.frames(), framesNow);
    }

    void theSceneOfAToolIsOverTheAppearanceUntilTheAppearanceIsSetAgain()
    {
        Rig rig;
        rig.hold();
        galleon::keyboard::Appearance appearance;
        appearance.colour = {40, 50, 60};
        QVERIFY(rig.controller->setAppearance(appearance));
        QTRY_VERIFY_WITH_TIMEOUT((ledColour(rig.frame(), 58) == std::vector<std::uint8_t>{40, 50, 60}), 3000);
        rig.advance(200);
        QVERIFY(rig.controller->SetLed(0x3a, 1, 2, 3)); // a tool lights one LED alone
        QTRY_VERIFY_WITH_TIMEOUT((ledColour(rig.frame(), 0x3a) == std::vector<std::uint8_t>{1, 2, 3}), 3000);
        QCOMPARE(ledColour(rig.frame(), 4), (std::vector<std::uint8_t>{0, 0, 0})); // and the rest is dark, not the appearance
        rig.advance(200);
        QVERIFY(rig.controller->setAppearance(appearance)); // asked for again: the tool is let go
        QTRY_VERIFY_WITH_TIMEOUT((ledColour(rig.frame(), 4) == std::vector<std::uint8_t>{40, 50, 60}), 3000);
    }

    void anAppearanceThatIsNotValidIsRefusedAndTheOneBeforeStays()
    {
        Rig rig;
        rig.hold();
        galleon::keyboard::Appearance good;
        good.colour = {9, 9, 9};
        QVERIFY(rig.controller->setAppearance(good));
        QTRY_VERIFY_WITH_TIMEOUT((ledColour(rig.frame(), 58) == std::vector<std::uint8_t>{9, 9, 9}), 3000);
        galleon::keyboard::Appearance wild;
        wild.level = 5000;
        QVERIFY(!rig.controller->setAppearance(wild));
        rig.advance(200);
        QTest::qWait(150);
        QCOMPARE(ledColour(rig.frame(), 58), (std::vector<std::uint8_t>{9, 9, 9}));
        QCOMPARE(rig.controller->appearance().colour.red, std::uint8_t{9});
    }

    void theAppearanceIsShownAgainWhenTheKeysAreTakenBackAfterASleep()
    {
        Rig rig;
        galleon::keyboard::Appearance appearance;
        appearance.colour = {11, 22, 33};
        QVERIFY(rig.controller->setAppearance(appearance));
        rig.hold();
        QTRY_VERIFY_WITH_TIMEOUT((ledColour(rig.frame(), 58) == std::vector<std::uint8_t>{11, 22, 33}), 3000);
        rig.power.sleep();
        QCOMPARE(rig.mode(), 1);
        {
            const std::scoped_lock lock(rig.device.mutex);
            rig.device.lastFrame.clear(); // what the keyboard showed is gone with the keys
        }
        rig.power.wake();
        QTRY_VERIFY_WITH_TIMEOUT(rig.controller->state() == QStringLiteral("listening"), 3000);
        rig.advance(4000);
        QTRY_VERIFY_WITH_TIMEOUT(rig.controller->state() == QStringLiteral("holding"), 3000);
        QTRY_VERIFY_WITH_TIMEOUT((ledColour(rig.frame(), 58) == std::vector<std::uint8_t>{11, 22, 33}), 3000);
    }

    void theAppearanceIsShownAgainWhenTheWebHubLeavesAndTheKeysAreTakenBack()
    {
        Rig rig;
        galleon::keyboard::Appearance appearance;
        appearance.colour = {70, 80, 90};
        QVERIFY(rig.controller->setAppearance(appearance));
        rig.hold();
        QTRY_VERIFY_WITH_TIMEOUT((ledColour(rig.frame(), 58) == std::vector<std::uint8_t>{70, 80, 90}), 3000);
        rig.device.webHubTakes(); // the Web Hub takes the keyboard to its own mode: it shows its own colours now
        rig.device.otherHostSpeaks();
        QTRY_VERIFY_WITH_TIMEOUT(rig.controller->state() == QStringLiteral("yielded"), 3000);
        {
            const std::scoped_lock lock(rig.device.mutex);
            rig.device.lastFrame.clear(); // what the keyboard shows is the Web Hub's now
        }
        // A minute of quiet: the keys are taken back, and the appearance is shown again (the player does not think it has already been shown).
        for (int i = 0; i < 50 && rig.controller->state() != QStringLiteral("holding"); ++i) {
            rig.advance(2000);
            QTest::qWait(120);
        }
        QTRY_VERIFY_WITH_TIMEOUT(rig.controller->state() == QStringLiteral("holding"), 3000);
        QTRY_VERIFY_WITH_TIMEOUT((ledColour(rig.frame(), 58) == std::vector<std::uint8_t>{70, 80, 90}), 3000);
    }

    // ---- the file of the appearance (lighting-appearance.json), applied by LightingAppearanceApplier

    void aGoodFileIsWhatTheKeysShowWhenTheLightingIsSwitchedOn()
    {
        const QTemporaryDir dir;
        const QString path = dir.filePath("lighting-appearance.json");
        galleon::keyboard::Appearance appearance;
        appearance.colour = {10, 20, 30};
        appearance.retouches = {{4, {255, 255, 255}}};
        QVERIFY(writeAppearance(path, appearance));
        Rig rig;
        const LightingAppearanceApplier applier(*rig.controller, path);
        QVERIFY(!rig.controller->active()); // a file never switches the lighting on
        rig.hold();
        QTRY_VERIFY_WITH_TIMEOUT((ledColour(rig.frame(), 58) == std::vector<std::uint8_t>{10, 20, 30}), 3000);
        QCOMPARE(ledColour(rig.frame(), 4), (std::vector<std::uint8_t>{255, 255, 255}));
    }

    void theJournalNamesTheSkyAndNeverWritesItsPlace()
    {
        const QTemporaryDir dir;
        const QString path = dir.filePath("lighting-appearance.json");
        QVERIFY(writeAppearance(path, skyAppearance(true, galleon::keyboard::GeoPlace{47.4147, 0.6828})));
        Rig rig;
        const LightingAppearanceApplier applier(*rig.controller, path);
        // The whole line, anchored: a place (or anything else) added to it would not match and the test would fail.
        QTest::ignoreMessage(QtInfoMsg, QRegularExpression(QStringLiteral(
            "^galleond: lighting: the appearance file was read: sky, following the sun, 0 retouches, level 1000 \\((applied|refused)\\)$")));
        rig.controller->Enable(true);
        QTRY_VERIFY_WITH_TIMEOUT(rig.controller->active(), 3000);
    }

    void theAnnouncerSaysEachAppearanceThatIsAcceptedAndNoneThatIsNot()
    {
        Rig rig;
        const QSignalSpy announced(&rig.controller->announcer(), &AppearanceAnnouncer::appearanceSet);
        QVERIFY(rig.controller->setAppearance(skyAppearance(true, galleon::keyboard::GeoPlace{48.8566, 2.3522})));
        QCOMPARE(announced.count(), 1);
        galleon::keyboard::Appearance invalid;
        invalid.level = 5000; // out of range
        QVERIFY(!rig.controller->setAppearance(invalid));
        QCOMPARE(announced.count(), 1);
    }

#ifndef Q_OS_WIN
    void noSignalOfTheControllerCarriesATypeThatTheBusDoesNotKnow()
    {
        // The controller is put on the D-Bus with all its signals: one with another type (an appearance, which may hold a place) would be refused with a
        // warning at each emission, and must never be offered to the bus.
        const QMetaObject &meta = LightingController::staticMetaObject;
        for (int index = meta.methodOffset(); index < meta.methodCount(); ++index) {
            const QMetaMethod method = meta.method(index);
            if (method.methodType() != QMetaMethod::Signal)
                continue;
            for (int parameter = 0; parameter < method.parameterCount(); ++parameter) {
                QVERIFY2(QDBusMetaType::typeToSignature(method.parameterMetaType(parameter)) != nullptr, method.methodSignature().constData());
            }
        }
    }
#endif

    // ---- the level in force (a curve of the day), over the level of the appearance

    void aLevelOverrideDimsWhatTheKeysShowAndGoingBackShowsTheAppearanceAgain()
    {
        Rig rig;
        galleon::keyboard::Appearance appearance;
        appearance.colour = {100, 200, 40};
        QVERIFY(rig.controller->setAppearance(appearance));
        rig.hold();
        QTRY_VERIFY_WITH_TIMEOUT((ledColour(rig.frame(), 58) == std::vector<std::uint8_t>{100, 200, 40}), 3000);
        QVERIFY(rig.controller->setLevelOverride(500));
        QTRY_VERIFY_WITH_TIMEOUT((ledColour(rig.frame(), 58) == std::vector<std::uint8_t>{50, 100, 20}), 3000);
        rig.advance(500); // the pacing of the frames follows the clock of the rig
        QVERIFY(rig.controller->setLevelOverride(std::nullopt));
        QTRY_VERIFY_WITH_TIMEOUT((ledColour(rig.frame(), 58) == std::vector<std::uint8_t>{100, 200, 40}), 3000);
    }

    void aLevelOverrideStaysOverALaterAppearanceAndIsKeptWhileOff()
    {
        Rig rig;
        QVERIFY(rig.controller->setLevelOverride(500)); // while the lighting is off
        galleon::keyboard::Appearance appearance;
        appearance.colour = {100, 200, 40};
        QVERIFY(rig.controller->setAppearance(appearance));
        rig.hold();
        QTRY_VERIFY_WITH_TIMEOUT((ledColour(rig.frame(), 58) == std::vector<std::uint8_t>{50, 100, 20}), 3000);
        appearance.colour = {200, 100, 20}; // a new appearance from the file: the level in force is still the curve's
        rig.advance(500);
        QVERIFY(rig.controller->setAppearance(appearance));
        QTRY_VERIFY_WITH_TIMEOUT((ledColour(rig.frame(), 58) == std::vector<std::uint8_t>{100, 50, 10}), 3000);
    }

    void aLevelOverrideOutOfRangeIsRefusedAndNothingChanges()
    {
        Rig rig;
        QVERIFY(!rig.controller->setLevelOverride(1001));
        QVERIFY(!rig.controller->setLevelOverride(-1));
        QCOMPARE(rig.controller->effectiveLevel(), 1000);
    }

    // ---- LightingLevelScheduler: the curve of the level through the day

    void inCurveModeTheLevelFollowsTheHourAndOtherwiseTheAppearanceKeepsItsOwn()
    {
        const QTemporaryDir dir;
        const QString path = dir.filePath("lighting-level-curve.json");
        galleond::BrightnessCurve curve;
        curve.mode = galleond::BrightnessCurve::Mode::Curve;
        curve.hours.fill(1000);
        curve.hours[10] = 300;
        curve.hours[11] = 600;
        QVERIFY(galleond::writeBrightnessCurve(path, curve));
        Rig rig;
        QTime now(10, 0);
        galleond::LightingLevelScheduler scheduler(*rig.controller, path, [&now] { return now; }, 3600000);
        QCOMPARE(rig.controller->effectiveLevel(), 300);
        now = QTime(11, 0);
        scheduler.evaluate();
        QCOMPARE(rig.controller->effectiveLevel(), 600);
        curve.mode = galleond::BrightnessCurve::Mode::Slider;
        QVERIFY(galleond::writeBrightnessCurve(path, curve));
        scheduler.reload();
        QCOMPARE(rig.controller->effectiveLevel(), 1000); // the slider: the level of the appearance file
    }

    void aMissingOrDamagedCurveFileChangesNothing()
    {
        const QTemporaryDir dir;
        const QString path = dir.filePath("lighting-level-curve.json");
        Rig rig;
        galleond::LightingLevelScheduler missing(*rig.controller, path, [] { return QTime(10, 0); }, 3600000);
        QCOMPARE(rig.controller->effectiveLevel(), 1000);
        galleond::BrightnessCurve curve;
        curve.mode = galleond::BrightnessCurve::Mode::Curve;
        curve.hours.fill(400);
        QVERIFY(galleond::writeBrightnessCurve(path, curve));
        missing.reload();
        QCOMPARE(rig.controller->effectiveLevel(), 400);
        QFile damaged(path);
        QVERIFY(damaged.open(QIODevice::WriteOnly | QIODevice::Truncate));
        damaged.write(R"({"version":1,"mode":)");
        damaged.close();
        missing.reload();
        QCOMPARE(rig.controller->effectiveLevel(), 400); // a half-written file changes nothing: the curve in force stays
    }

    // ---- the hidden green rain on the keys (the easter egg)

    void theGreenRainTakesTheKeysWhileItRunsAndTheAppearanceComesBackAfterwards()
    {
        Rig rig;
        galleon::keyboard::Appearance appearance;
        appearance.colour = {10, 20, 30};
        QVERIFY(rig.controller->setAppearance(appearance));
        rig.hold();
        QTRY_VERIFY_WITH_TIMEOUT((ledColour(rig.frame(), 58) == std::vector<std::uint8_t>{10, 20, 30}), 3000);
        QVERIFY(rig.controller->setMatrixRain(true));
        QVERIFY(rig.controller->matrixRain());
        // Some key has the almost white head of the rain and some others the greens of its trail (the picture moves, so look for a few moments).
        const auto hasHeadAndTrail = [&rig] {
            rig.advance(120);
            const auto frame = rig.frame();
            bool head = false;
            bool trail = false;
            for (int led = 0; led < 146; ++led) {
                const auto c = ledColour(frame, led);
                if (c.size() == 3 && c[0] == 216 && c[1] == 255 && c[2] == 224)
                    head = true;
                if (c.size() == 3 && c[0] == 0 && c[1] > 0 && c[1] < 213 && c[2] <= c[1])
                    trail = true;
            }
            return head && trail;
        };
        QTRY_VERIFY_WITH_TIMEOUT(hasHeadAndTrail(), 5000);
        // It stops: the keys show the appearance again.
        rig.advance(500);
        QVERIFY(rig.controller->setMatrixRain(false));
        QVERIFY(!rig.controller->matrixRain());
        QTRY_VERIFY_WITH_TIMEOUT((ledColour(rig.frame(), 58) == std::vector<std::uint8_t>{10, 20, 30}), 3000);
    }

    void theGreenRainDoesNothingWhileTheLightingIsOffAndIsLetGoWhenItIsSwitchedOff()
    {
        Rig rig;
        QVERIFY(!rig.controller->setMatrixRain(true)); // the person did not switch the lighting on: nothing is lit for the easter egg
        QVERIFY(!rig.controller->matrixRain());
        rig.hold();
        QVERIFY(rig.controller->setMatrixRain(true));
        QVERIFY(rig.controller->Enable(false).isEmpty());
        QVERIFY(!rig.controller->matrixRain()); // let go with the lighting
        rig.hold();                             // switched on again: the appearance, not the rain
        QVERIFY(!rig.controller->matrixRain());
    }

    void anAppearanceSetWhileTheRainRunsIsKeptForWhenItStops()
    {
        Rig rig;
        rig.hold();
        QVERIFY(rig.controller->setMatrixRain(true));
        galleon::keyboard::Appearance appearance;
        appearance.colour = {70, 80, 90};
        QVERIFY(rig.controller->setAppearance(appearance));
        QVERIFY(rig.controller->matrixRain()); // still raining
        rig.advance(500);
        QVERIFY(rig.controller->setMatrixRain(false));
        QTRY_VERIFY_WITH_TIMEOUT((ledColour(rig.frame(), 58) == std::vector<std::uint8_t>{70, 80, 90}), 3000);
    }

    // ---- the sky of the hour

    void theSkyShowsTheColoursOfTheHourOnTheRealKeysAndFollowsTheHour()
    {
        // Declared before the rig: the worker thread reads it until the rig is gone.
        std::atomic<double> hour{12.0};
        Rig rig;
        rig.controller->setDayHours([&hour] { return hour.load(); });
        galleon::keyboard::Appearance appearance;
        appearance.base = galleon::keyboard::BaseType::Sky;
        appearance.skyAnimated = false; // the colours of the hour alone
        QVERIFY(rig.controller->setAppearance(appearance));
        rig.hold();
        // F1 (LED 58) is on the top row, the A key of a QWERTY (LED 4) on the fourth: at noon the colours of the day.
        QTRY_VERIFY_WITH_TIMEOUT((ledColour(rig.frame(), 58) == std::vector<std::uint8_t>{26, 70, 170}), 3000);
        QCOMPARE(ledColour(rig.frame(), 4), (std::vector<std::uint8_t>{44, 110, 215}));
        QCOMPARE(ledColour(rig.frame(), 141), (std::vector<std::uint8_t>{16, 42, 102})); // a section of the strip: the top row dimmed
        // The dusk comes: the next frame is the colours of 18 h, the keys follow the clock of the computer.
        rig.advance(500);
        hour = 18.0;
        QTRY_VERIFY_WITH_TIMEOUT((ledColour(rig.frame(), 58) == std::vector<std::uint8_t>{30, 22, 90}), 3000);
        QCOMPARE(ledColour(rig.frame(), 4), (std::vector<std::uint8_t>{210, 60, 90}));
    }

    // ---- the sky that follows the sun: the place of the weather widget first, then the one of the sky, the fixed hours when there is none

    void theSkyFollowsTheRealSunriseAndSunsetWhenAskedAndAPlaceIsKnown()
    {
        const QTemporaryDir dir;
        writeWidgetsPlace(dir.filePath("widgets.json"), 48.8566, 2.3522); // Paris
        Rig rig;
        const LightingSkyDay skyDay(*rig.controller, dir.filePath("widgets.json"), parisSolstice);
        const double sunset = galleon::keyboard::sunDay(48.8566, 2.3522, 2026, 6, 21, 2.0).sunsetHours;
        rig.controller->setDayHours([sunset] { return sunset; }); // a copy: the worker thread outlives this function's locals
        QVERIFY(rig.controller->setAppearance(skyAppearance(true)));
        rig.hold();
        // At the real sunset of Paris, the top row (F1, LED 58) has the colours of the dusk, not those of the night that the fixed hours would give.
        QTRY_VERIFY_WITH_TIMEOUT((ledColour(rig.frame(), 58) == std::vector<std::uint8_t>{30, 22, 90}), 3000);
    }

    void theSkyOfTheWorkerThreadStaysValidWhenTheObjectThatMadeItIsDestroyedFirst()
    {
        // At the end of main() the object is destroyed before the controller, whose worker thread still calls the function it was given each step:
        // that function must not depend on the object. (Under the sanitizers the old version failed here with a call on a destroyed object.)
        const QTemporaryDir dir;
        writeWidgetsPlace(dir.filePath("widgets.json"), 48.8566, 2.3522); // Paris
        const double sunset = galleon::keyboard::sunDay(48.8566, 2.3522, 2026, 6, 21, 2.0).sunsetHours;
        const std::atomic<double> hour{sunset}; // declared before the rig: the worker reads it until the rig is gone
        Rig rig;
        rig.controller->setDayHours([&hour] { return hour.load(); });
        {
            const LightingSkyDay skyDay(*rig.controller, dir.filePath("widgets.json"), parisSolstice);
            QVERIFY(rig.controller->setAppearance(skyAppearance(true)));
            rig.hold();
            QTRY_VERIFY_WITH_TIMEOUT((ledColour(rig.frame(), 58) == std::vector<std::uint8_t>{30, 22, 90}), 3000); // the real dusk of Paris
        } // destroyed: the worker is still running
        rig.advance(500);
        QTest::qWait(300); // a few steps of the worker with the object gone
        QTRY_VERIFY_WITH_TIMEOUT((ledColour(rig.frame(), 58) == std::vector<std::uint8_t>{30, 22, 90}), 3000); // still the dusk
    }

    void theSkyDoesNotFollowTheSunUnlessAskedAndThenKeepsTheFixedHours()
    {
        const QTemporaryDir dir;
        writeWidgetsPlace(dir.filePath("widgets.json"), 48.8566, 2.3522);
        Rig rig;
        const LightingSkyDay skyDay(*rig.controller, dir.filePath("widgets.json"), parisSolstice);
        const double sunset = galleon::keyboard::sunDay(48.8566, 2.3522, 2026, 6, 21, 2.0).sunsetHours;
        rig.controller->setDayHours([sunset] { return sunset; });
        QVERIFY(rig.controller->setAppearance(skyAppearance(false)));
        rig.hold();
        // The fixed hours: 21 h 58 is well into the night of the mockup.
        QTRY_VERIFY_WITH_TIMEOUT((ledColour(rig.frame(), 58) == std::vector<std::uint8_t>{10, 14, 38}), 3000);
        QCOMPARE(skyDay.weatherReads(), 0); // privacy: the file of the weather widget was not even read
    }

    void theWeatherPlaceComesBeforeThePlaceOfTheSkySettingsAndTheSkyPlaceIsUsedWhenThereIsNone()
    {
        const QTemporaryDir dir;
        const double sunsetParis = galleon::keyboard::sunDay(48.8566, 2.3522, 2026, 6, 21, 2.0).sunsetHours;
        // The place of the sky settings alone (the weather was never used: no widgets.json): Paris.
        {
            Rig rig;
            const LightingSkyDay skyDay(*rig.controller, dir.filePath("none.json"), parisSolstice);
            rig.controller->setDayHours([sunsetParis] { return sunsetParis; });
            QVERIFY(rig.controller->setAppearance(skyAppearance(true, galleon::keyboard::GeoPlace{48.8566, 2.3522})));
            QCOMPARE(skyDay.day(), galleond::skyDayFor(true, galleon::keyboard::GeoPlace{48.8566, 2.3522}, QDate(2026, 6, 21), 2.0));
            QVERIFY(skyDay.day() != galleon::keyboard::SkyDay{}); // the real sun, not the fixed schedule
        }
        // Both: the weather widget wins (Sydney in the file, Paris in the settings of the sky).
        {
            writeWidgetsPlace(dir.filePath("widgets.json"), -33.8688, 151.2093);
            Rig rig;
            const LightingSkyDay skyDay(*rig.controller, dir.filePath("widgets.json"), parisSolstice);
            QVERIFY(rig.controller->setAppearance(skyAppearance(true, galleon::keyboard::GeoPlace{48.8566, 2.3522})));
            QCOMPARE(skyDay.day(), galleond::skyDayFor(true, galleon::keyboard::GeoPlace{-33.8688, 151.2093}, QDate(2026, 6, 21), 2.0));
        }
        // A weather place that is not a place (damaged file) is as good as none: the one of the sky settings.
        {
            QFile out(dir.filePath("widgets.json"));
            QVERIFY(out.open(QIODevice::WriteOnly | QIODevice::Truncate));
            out.write(R"({"location":{"latitude":999,"longitude":2.3}})");
            out.close();
            Rig rig;
            const LightingSkyDay skyDay(*rig.controller, dir.filePath("widgets.json"), parisSolstice);
            QVERIFY(rig.controller->setAppearance(skyAppearance(true, galleon::keyboard::GeoPlace{48.8566, 2.3522})));
            QCOMPARE(skyDay.day(), galleond::skyDayFor(true, galleon::keyboard::GeoPlace{48.8566, 2.3522}, QDate(2026, 6, 21), 2.0));
        }
        // No place at all: the fixed schedule, even though the sky asks to follow the sun.
        {
            Rig rig;
            const LightingSkyDay skyDay(*rig.controller, dir.filePath("absent.json"), parisSolstice);
            QVERIFY(rig.controller->setAppearance(skyAppearance(true)));
            QCOMPARE(skyDay.day(), galleon::keyboard::SkyDay{});
        }
    }

    void theWeatherFileIsReadAgainWhenItChangesButOnlyWhileTheSkyFollowsTheSun()
    {
        const QTemporaryDir dir;
        const QString widgets = dir.filePath("widgets.json");
        writeWidgetsPlace(widgets, 48.8566, 2.3522);
        Rig rig;
        const LightingSkyDay skyDay(*rig.controller, widgets, parisSolstice);
        QCOMPARE(skyDay.weatherReads(), 0);
        QVERIFY(rig.controller->setAppearance(skyAppearance(true)));
        QCOMPARE(skyDay.weatherReads(), 1); // read when the sky starts to follow the sun
        writeWidgetsPlace(widgets, -33.8688, 151.2093);
        QTRY_VERIFY_WITH_TIMEOUT(skyDay.weatherReads() >= 2, 5000); // the file was replaced: the new place is read
        QCOMPARE(skyDay.day(), galleond::skyDayFor(true, galleon::keyboard::GeoPlace{-33.8688, 151.2093}, QDate(2026, 6, 21), 2.0));
        // The sky stops following the sun: the place is forgotten and the file is not read any more.
        QVERIFY(rig.controller->setAppearance(skyAppearance(false)));
        const int reads = skyDay.weatherReads();
        writeWidgetsPlace(widgets, 35.6762, 139.6503);
        QTest::qWait(600);
        QCOMPARE(skyDay.weatherReads(), reads);
        QCOMPARE(skyDay.day(), galleon::keyboard::SkyDay{});
    }

    void theFixedRainbowOfTheFileIsWhatTheKeysShowWithTheRealPositions()
    {
        // The file as the owner wrote it by hand, read from the disk, and the positions built into galleond.
        const QTemporaryDir dir;
        const QString path = dir.filePath("lighting-appearance.json");
        QFile out(path);
        QVERIFY(out.open(QIODevice::WriteOnly));
        out.write(R"({"version":1,"base":{"type":"rainbow","speed":0},"overrides":[],"level":1000})");
        out.close();
        Rig rig;
        const LightingAppearanceApplier applier(*rig.controller, path);
        rig.hold();
        QCOMPARE(rig.controller->state(), QStringLiteral("holding"));
        QTRY_VERIFY_WITH_TIMEOUT(rig.frames() >= 1, 3000);
        // F1 (LED 58) is lit with a colour of the rainbow, not dark; the left end is red-ish, the right end magenta-ish.
        const auto f1 = ledColour(rig.frame(), 58);
        QVERIFY(!f1.empty());
        QVERIFY2(f1 != (std::vector<std::uint8_t>{0, 0, 0}), "F1 has to be lit by the rainbow");
        int lit = 0;
        for (int led = 0; led < 146; ++led)
            lit += ledColour(rig.frame(), led) != std::vector<std::uint8_t>{0, 0, 0} ? 1 : 0;
        QCOMPARE(lit, 95); // the 88 keys and the 7 sections of the strip
    }

    void withoutAFileTheKeysAreDarkWhenTheLightingIsSwitchedOn()
    {
        const QTemporaryDir dir;
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression("the appearance file is not used"));
        Rig rig;
        const LightingAppearanceApplier applier(*rig.controller, dir.filePath("lighting-appearance.json"));
        rig.hold();
        QTRY_VERIFY_WITH_TIMEOUT(rig.frames() >= 1, 3000);
        QCOMPARE(ledColour(rig.frame(), 58), (std::vector<std::uint8_t>{0, 0, 0}));
    }

    void aFileThatChangesWhileTheLightingIsOnIsAppliedAtOnce()
    {
        const QTemporaryDir dir;
        const QString path = dir.filePath("lighting-appearance.json");
        galleon::keyboard::Appearance first;
        first.colour = {1, 2, 3};
        QVERIFY(writeAppearance(path, first));
        Rig rig;
        const LightingAppearanceApplier applier(*rig.controller, path);
        rig.hold();
        QTRY_VERIFY_WITH_TIMEOUT((ledColour(rig.frame(), 58) == std::vector<std::uint8_t>{1, 2, 3}), 3000);
        galleon::keyboard::Appearance second;
        second.colour = {7, 8, 9};
        QVERIFY(writeAppearance(path, second));
        rig.advance(200);
        // The watcher of the folder says so by itself; the clock moves on so that the frame is not held back for pacing.
        for (int i = 0; i < 60 && ledColour(rig.frame(), 58) != std::vector<std::uint8_t>{7, 8, 9}; ++i) {
            rig.advance(60);
            QTest::qWait(100);
        }
        QCOMPARE(ledColour(rig.frame(), 58), (std::vector<std::uint8_t>{7, 8, 9}));
    }

    void aFileThatBecomesInvalidWhileTheLightingIsOnIsIgnoredAndWhatIsShownStays()
    {
        const QTemporaryDir dir;
        const QString path = dir.filePath("lighting-appearance.json");
        galleon::keyboard::Appearance good;
        good.colour = {40, 50, 60};
        QVERIFY(writeAppearance(path, good));
        Rig rig;
        LightingAppearanceApplier applier(*rig.controller, path);
        rig.hold();
        QTRY_VERIFY_WITH_TIMEOUT((ledColour(rig.frame(), 58) == std::vector<std::uint8_t>{40, 50, 60}), 3000);
        QFile broken(path);
        QVERIFY(broken.open(QIODevice::WriteOnly | QIODevice::Truncate));
        broken.write("{ half a file");
        broken.close();
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression("the appearance file is not used"));
        applier.onFileMaybeChanged();
        rig.advance(300);
        QTest::qWait(200);
        QCOMPARE(ledColour(rig.frame(), 58), (std::vector<std::uint8_t>{40, 50, 60})); // not dark: a damaged file does not make the keys flash
        QCOMPARE(rig.controller->appearance().colour.red, std::uint8_t{40});
    }

    void aChangeOfTheFileWhileTheLightingIsOffIsReadWhenItIsSwitchedOn()
    {
        const QTemporaryDir dir;
        const QString path = dir.filePath("lighting-appearance.json");
        Rig rig;
        LightingAppearanceApplier applier(*rig.controller, path);
        galleon::keyboard::Appearance appearance;
        appearance.colour = {12, 13, 14};
        QVERIFY(writeAppearance(path, appearance));
        applier.onFileMaybeChanged();
        QCOMPARE(rig.controller->appearance().colour.red, std::uint8_t{0}); // off: nothing was read
        rig.hold();
        QTRY_VERIFY_WITH_TIMEOUT((ledColour(rig.frame(), 58) == std::vector<std::uint8_t>{12, 13, 14}), 3000);
    }

    void anInvalidFileAtSwitchOnGivesDarkKeysAndNotWhatWasShownBefore()
    {
        const QTemporaryDir dir;
        const QString path = dir.filePath("lighting-appearance.json");
        galleon::keyboard::Appearance appearance;
        appearance.colour = {90, 91, 92};
        QVERIFY(writeAppearance(path, appearance));
        Rig rig;
        const LightingAppearanceApplier applier(*rig.controller, path);
        rig.hold();
        QTRY_VERIFY_WITH_TIMEOUT((ledColour(rig.frame(), 58) == std::vector<std::uint8_t>{90, 91, 92}), 3000);
        QCOMPARE(rig.controller->Enable(false), QString());
        QFile broken(path);
        QVERIFY(broken.open(QIODevice::WriteOnly | QIODevice::Truncate));
        broken.write("not json");
        broken.close();
        {
            const std::scoped_lock lock(rig.device.mutex);
            rig.device.lastFrame.clear();
        }
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression("the appearance file is not used"));
        rig.hold();
        QTRY_VERIFY_WITH_TIMEOUT((ledColour(rig.frame(), 58) == std::vector<std::uint8_t>{0, 0, 0}), 3000);
    }

    void aToolSceneIsNotCarriedOverASwitchingOff()
    {
        Rig rig;
        rig.hold();
        QVERIFY(rig.controller->SetLed(0x3a, 1, 2, 3));
        QTRY_VERIFY_WITH_TIMEOUT((ledColour(rig.frame(), 0x3a) == std::vector<std::uint8_t>{1, 2, 3}), 3000);
        QCOMPARE(rig.controller->Enable(false), QString());
        {
            const std::scoped_lock lock(rig.device.mutex);
            rig.device.lastFrame.clear(); // only what is sent from now on counts
        }
        rig.hold(); // switched on again, nothing asked for since: what the tool showed is not there, the keys are dark
        QTRY_VERIFY_WITH_TIMEOUT((ledColour(rig.frame(), 0x3a) == std::vector<std::uint8_t>{0, 0, 0}), 3000);
    }

    void oneColourPerLedShowsEachLedItsOwnColourAndTheOthersBlack()
    {
        Rig rig;
        rig.hold();
        QCOMPARE(rig.controller->state(), QStringLiteral("holding"));
        QVERIFY(rig.controller->SetLeds({0x3a, 255, 0, 0, 4, 0, 255, 0, 145, 1, 2, 3}));
        QTRY_VERIFY_WITH_TIMEOUT((ledColour(rig.frame(), 0x3a) == std::vector<std::uint8_t>{255, 0, 0}), 3000);
        QCOMPARE(ledColour(rig.frame(), 4), (std::vector<std::uint8_t>{0, 255, 0}));
        QCOMPARE(ledColour(rig.frame(), 145), (std::vector<std::uint8_t>{1, 2, 3}));
        QCOMPARE(ledColour(rig.frame(), 0), (std::vector<std::uint8_t>{0, 0, 0}));
        QCOMPARE(ledColour(rig.frame(), 0x3b), (std::vector<std::uint8_t>{0, 0, 0}));
    }

    void aListOfLedsThatIsNotWellFormedIsRefusedAndTheLastSceneStays()
    {
        Rig rig;
        rig.hold();
        QVERIFY(rig.controller->SetColor(7, 8, 9));
        QVERIFY(!rig.controller->SetLeds({}));                       // nothing to show
        QVERIFY(!rig.controller->SetLeds({1, 2, 3}));                // not groups of four
        QVERIFY(!rig.controller->SetLeds({146, 1, 1, 1}));           // a LED beyond the frame
        QVERIFY(!rig.controller->SetLeds({-1, 1, 1, 1}));
        QVERIFY(!rig.controller->SetLeds({5, 256, 0, 0}));           // a colour out of range
        QVERIFY(!rig.controller->SetLeds({5, 0, -1, 0}));
        QList<int> tooMany;
        for (int i = 0; i < 147; ++i)
            tooMany << 3 << 1 << 1 << 1;
        QVERIFY(!rig.controller->SetLeds(tooMany));                  // more groups than LEDs
        rig.advance(200);
        QTest::qWait(150);
        QCOMPARE(ledColour(rig.frame(), 0x3a), (std::vector<std::uint8_t>{7, 8, 9})); // none of them changed what is shown
    }

    void oneColourPerLedIsRefusedWhileTheLightingIsOff()
    {
        Rig rig;
        QVERIFY(!rig.controller->SetLeds({5, 1, 1, 1}));
    }

    void oneLedIsRefusedWhileTheLightingIsOff()
    {
        Rig rig;
        QVERIFY(!rig.controller->SetLed(5, 1, 1, 1)); // not enabled: nothing is sent and nobody is told it worked
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

    void aKeyboardNotBackYetWhenTheMachineWakesIsWaitedForAndTheLightingComesBack()
    {
        Rig rig;
        rig.controller->setResumeRetry(30, 200);
        rig.hold();
        rig.power.sleep();
        rig.keyboardPresent = false; // still re-enumerating after the resume
        rig.power.wake();
        QTest::qWait(300); // several tries, all failed
        QVERIFY(rig.controller->active()); // one failed opening did not switch the lighting off for good
        QCOMPARE(rig.controller->state(), QStringLiteral("starting"));
        rig.keyboardPresent = true;
        QTRY_VERIFY_WITH_TIMEOUT(rig.controller->state() == QStringLiteral("listening"), 3000);
        rig.advance(4000);
        QTRY_VERIFY_WITH_TIMEOUT(rig.controller->state() == QStringLiteral("holding"), 3000);
        QCOMPARE(rig.mode(), 2);
        QCOMPARE(rig.journal.count("watchdog.start"), 1);
    }

    void aKeyboardThatNeverComesBackAfterWakingEndsInOffAfterTheTries()
    {
        Rig rig;
        rig.controller->setResumeRetry(20, 3);
        rig.hold();
        rig.power.sleep();
        rig.keyboardPresent = false;
        rig.power.wake();
        QTRY_VERIFY_WITH_TIMEOUT(!rig.controller->active(), 3000);
        QCOMPARE(rig.controller->state(), QStringLiteral("off"));
        QCOMPARE(rig.journal.count("opener"), 1 + 1 + 3); // the first start, the wake, then the three tries
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
