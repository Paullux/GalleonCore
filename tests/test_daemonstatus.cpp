// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#include <DaemonStatus.h>

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <memory>

using bosun::DaemonStatus;

namespace {

// A program in place of systemctl: it writes its arguments to a file.
QString fakeSystemctl(const QString &dir)
{
    QString script = dir + "/systemctl";
    QFile file(script);
    if (!file.open(QIODevice::WriteOnly))
        return {};
    file.write(("#!/bin/sh\necho \"$@\" >> " + dir + "/asked\n").toUtf8());
    file.close();
    QFile::setPermissions(script, QFile::permissions(script) | QFile::ExeUser);
    return script;
}

QString asked(const QString &dir)
{
    QFile file(dir + "/asked");
    return file.open(QIODevice::ReadOnly) ? QString::fromUtf8(file.readAll()) : QString();
}

} // namespace

class GamingEndpoint : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.galleoncore.Deck")
public:
    int calls = 0;
public Q_SLOTS:
    bool ShowGaming() { ++calls; return true; }
};

// What galleond's org.galleoncore.Lighting object does, as far as Bosun can tell: it answers and it tells its state.
class LightingEndpoint : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.galleoncore.Lighting")
public:
    QString refusal; // when not empty, Enable(true) refuses with this
    QString state = QStringLiteral("off");
    QList<bool> enabled;
    QList<QList<int>> colours;
public Q_SLOTS:
    QString Enable(bool on)
    {
        enabled.append(on);
        if (on && !refusal.isEmpty())
            return refusal;
        state = on ? QStringLiteral("starting") : QStringLiteral("off");
        return {};
    }
    bool SetColor(int red, int green, int blue) { colours.append({red, green, blue}); return true; }
    QString State() const { return state; }
Q_SIGNALS:
    void stateChanged(const QString &state, const QString &detail);
};

// galleond as another program: its own connection to the bus (a connection never gets its own signals back), with the
// name and the lighting object, and everything taken down again whatever the test does, so that one failure does not
// leave the name taken for the tests after it.
struct FakeDaemon
{
    explicit FakeDaemon(const QString &connectionName, bool ownsTheName = true)
        : name(connectionName), connection(QDBusConnection::connectToBus(QDBusConnection::SessionBus, connectionName))
    {
        ok = connection.registerObject("/org/galleoncore/Lighting", &endpoint, QDBusConnection::ExportAllSlots | QDBusConnection::ExportAllSignals);
        if (ownsTheName)
            ok = ok && connection.registerService(DaemonStatus::BusName);
    }
    ~FakeDaemon()
    {
        connection.unregisterObject("/org/galleoncore/Lighting");
        connection.unregisterService(DaemonStatus::BusName);
        QDBusConnection::disconnectFromBus(name);
    }
    FakeDaemon(const FakeDaemon &) = delete;
    FakeDaemon &operator=(const FakeDaemon &) = delete;

    QString name;
    QDBusConnection connection;
    LightingEndpoint endpoint;
    bool ok = false;
};

class TestDaemonStatus : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void aBrightnessReadFromTheKeyboardBecomesTheNearestStopOfTheSlider()
    {
        QCOMPARE(DaemonStatus::nearestBrightnessStop(0), 0);
        QCOMPARE(DaemonStatus::nearestBrightnessStop(1000), 1000);
        QCOMPARE(DaemonStatus::nearestBrightnessStop(700), 700);
        QCOMPARE(DaemonStatus::nearestBrightnessStop(449), 400); // set by another program: nearest stop
        QCOMPARE(DaemonStatus::nearestBrightnessStop(450), 500);
        QCOMPARE(DaemonStatus::nearestBrightnessStop(-1), -1);
        QCOMPARE(DaemonStatus::nearestBrightnessStop(1001), -1);
    }

    void showingGamingCallsTheDaemonWithoutStartingAnotherProcess()
    {
        // galleond is another program: its own connection, so that this one never caches itself as the owner of the name.
        const QString other = QStringLiteral("fake-galleond-deck");
        QDBusConnection bus = QDBusConnection::connectToBus(QDBusConnection::SessionBus, other);
        GamingEndpoint endpoint;
        QVERIFY(bus.registerService(DaemonStatus::BusName));
        QVERIFY(bus.registerObject("/org/galleoncore/Deck", &endpoint, QDBusConnection::ExportAllSlots));
        {
            DaemonStatus status("/nonexistent/systemctl");
            QTRY_VERIFY_WITH_TIMEOUT(status.running(), 3000);
            QVERIFY(status.showGaming().isEmpty());
            QTRY_COMPARE(endpoint.calls, 1);
        }
        bus.unregisterObject("/org/galleoncore/Deck");
        QVERIFY(bus.unregisterService(DaemonStatus::BusName));
        QDBusConnection::disconnectFromBus(other);
    }
    void lightingIsOffAndSaysSoBeforeAnyDaemonIsThere()
    {
        DaemonStatus status("/nonexistent/systemctl");
        QCOMPARE(status.lightingState(), QStringLiteral("off"));
        QVERIFY(!status.lightingActive());
        QVERIFY(status.lightingSupported());
        QVERIFY(!status.enableLighting(true).isEmpty()); // no daemon: it says why, and sends nothing
        QVERIFY(!status.setLightingColor(1, 2, 3).isEmpty());
    }

    void switchingTheLightingOnAsksTheDaemonAndFollowsItsState()
    {
        FakeDaemon daemon(QStringLiteral("fake-galleond-1"));
        QVERIFY(daemon.ok);
        DaemonStatus status("/nonexistent/systemctl");
        QTRY_VERIFY_WITH_TIMEOUT(status.running(), 3000);
        const QSignalSpy changed(&status, &DaemonStatus::lightingChanged);

        QVERIFY(status.enableLighting(true).isEmpty());
        QTRY_COMPARE(daemon.endpoint.enabled, QList<bool>{true});
        // The daemon says where it is, as it does at every change; Qt follows the new owner of the name with a short delay,
        // so it says it again until it is heard.
        for (int i = 0; i < 40 && status.lightingState() != QStringLiteral("listening"); ++i) {
            Q_EMIT daemon.endpoint.stateChanged(QStringLiteral("listening"), {});
            QTest::qWait(50);
        }
        QCOMPARE(status.lightingState(), QStringLiteral("listening"));
        QVERIFY(status.lightingActive());
        QVERIFY(changed.size() >= 1);

        QVERIFY(status.setLightingColor(10, 20, 30).isEmpty());
        QTRY_COMPARE(daemon.endpoint.colours.size(), 1);
        QCOMPARE(daemon.endpoint.colours.constFirst(), (QList<int>{10, 20, 30}));
        QVERIFY(!status.setLightingColor(256, 0, 0).isEmpty()); // refused here, never sent
        QVERIFY(!status.setLightingColor(0, -1, 0).isEmpty());
        QTest::qWait(100);
        QCOMPARE(daemon.endpoint.colours.size(), 1);

        QVERIFY(status.enableLighting(false).isEmpty());
        QTRY_COMPARE(daemon.endpoint.enabled, (QList<bool>{true, false}));
        QTRY_VERIFY(!status.lightingActive());
    }

    void aRefusalOfTheDaemonShowsItsReasonAndLeavesTheLightingOff()
    {
        FakeDaemon daemon(QStringLiteral("fake-galleond-2"));
        QVERIFY(daemon.ok);
        daemon.endpoint.refusal = QStringLiteral("the watchdog program is not installed");
        DaemonStatus status("/nonexistent/systemctl");
        QTRY_VERIFY_WITH_TIMEOUT(status.running(), 3000);
        QVERIFY(status.enableLighting(true).isEmpty()); // sent
        QTRY_COMPARE(status.lightingDetail(), QStringLiteral("the watchdog program is not installed"));
        QCOMPARE(status.lightingState(), QStringLiteral("off"));
        QVERIFY(!status.lightingActive());
    }

    void aDaemonAlreadyHoldingTheLightingIsFoundWhenBosunStartsAfterIt()
    {
        FakeDaemon daemon(QStringLiteral("fake-galleond-3"));
        QVERIFY(daemon.ok);
        daemon.endpoint.state = QStringLiteral("holding");
        const DaemonStatus status("/nonexistent/systemctl");
        QTRY_COMPARE_WITH_TIMEOUT(status.lightingState(), QStringLiteral("holding"), 3000);
    }

    void whenTheDaemonGoesAwayTheLightingIsOffForBosun()
    {
        auto daemon = std::make_unique<FakeDaemon>(QStringLiteral("fake-galleond-4"));
        QVERIFY(daemon->ok);
        daemon->endpoint.state = QStringLiteral("holding");
        const DaemonStatus status("/nonexistent/systemctl");
        QTRY_VERIFY_WITH_TIMEOUT(status.lightingActive(), 3000);
        daemon.reset(); // the daemon ends: its name is gone
        QTRY_VERIFY_WITH_TIMEOUT(!status.lightingActive(), 3000);
    }

    void theCommandAsksTheServiceOfTheUserAndNothingElse()
    {
        QCOMPARE(DaemonStatus::userCommand("start"), (QStringList{"--user", "start", "galleond.service"}));
        QCOMPARE(DaemonStatus::userCommand("stop"), (QStringList{"--user", "stop", "galleond.service"}));
    }

    void itFollowsTheNameOnTheBus()
    {
        QDBusConnection bus = QDBusConnection::sessionBus();
        QVERIFY(bus.isConnected());
        QVERIFY(!bus.interface()->isServiceRegistered(DaemonStatus::BusName)); // a private bus: nobody has it
        const DaemonStatus status("/nonexistent/systemctl");
        QVERIFY(!status.running());
        const QSignalSpy changed(&status, &DaemonStatus::changed);
        QVERIFY(bus.registerService(DaemonStatus::BusName));
        QTRY_VERIFY_WITH_TIMEOUT(status.running(), 3000);
        QCOMPARE(changed.size(), 1);
        QVERIFY(bus.unregisterService(DaemonStatus::BusName));
        QTRY_VERIFY_WITH_TIMEOUT(!status.running(), 3000);
        QCOMPARE(changed.size(), 2);
    }

    void startingAsksSystemdOnlyWhenItIsNotRunning()
    {
        const QTemporaryDir dir;
        DaemonStatus status(fakeSystemctl(dir.path()));
        QVERIFY(status.start().isEmpty());
        QTRY_COMPARE_WITH_TIMEOUT(asked(dir.path()), QStringLiteral("--user start galleond.service\n"), 3000);
        QVERIFY(status.stop().isEmpty()); // not running: nothing to stop, nothing asked
        QTest::qWait(300);
        QCOMPARE(asked(dir.path()), QStringLiteral("--user start galleond.service\n"));
    }

    void stoppingAsksSystemdWhenItIsRunning()
    {
        const QTemporaryDir dir;
        QDBusConnection bus = QDBusConnection::sessionBus();
        QVERIFY(bus.registerService(DaemonStatus::BusName));
        DaemonStatus status(fakeSystemctl(dir.path()));
        QVERIFY(status.running());
        QVERIFY(status.start().isEmpty()); // already running: nothing asked
        QVERIFY(status.stop().isEmpty());
        QTRY_COMPARE_WITH_TIMEOUT(asked(dir.path()), QStringLiteral("--user stop galleond.service\n"), 3000);
        QVERIFY(bus.unregisterService(DaemonStatus::BusName));
    }

    void aMissingSystemctlIsSaid()
    {
        DaemonStatus status("/nonexistent/systemctl");
        QVERIFY(!status.start().isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestDaemonStatus)
#include "test_daemonstatus.moc"
