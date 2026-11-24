// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

// galleon-key-signal: its arguments, what it sends to the local endpoint (Windows), the signal it holds on the bus (Linux) and the program itself against a fake galleond.
#include "EndpointSend.h"
#include "KeySignalArgs.h"
#ifndef Q_OS_WIN
#include "HeldSignal.h"
#endif

#include <LocalControl.h>

#include <QLocalServer>
#include <QLocalSocket>
#include <QProcess>
#include <QSignalSpy>
#include <QTest>
#include <QThread>
#include <QTimer>
#include <QUuid>

#include <memory>
#ifndef Q_OS_WIN
#include <QDBusConnection>
#endif

using namespace keysignal;

namespace {
// The arguments of a parse that was checked to have some (the check is a QVERIFY just before): a plain value, so that nothing here reads an empty optional.
Arguments argsOf(const ParseResult &result)
{
    return result.arguments.value_or(Arguments{});
}
} // namespace

#ifndef Q_OS_WIN
// The key signals interface of galleond, as a fake.
class FakeKeySignals : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.galleoncore.KeySignals")
public:
    struct Call
    {
        QString key;
        int red = 0;
        int green = 0;
        int blue = 0;
        QString effect;
        double seconds = 0;
    };
    QList<Call> sets;
    QStringList clears;
    QString answer; // what Set says: "" when accepted, else why not
public Q_SLOTS:
    QString Set(const QString &key, int red, int green, int blue, const QString &effect, double seconds)
    {
        sets.append({key, red, green, blue, effect, seconds});
        return answer;
    }
    bool Clear(const QString &key)
    {
        clears.append(key);
        return true;
    }
    bool ClearAll() { return true; }
};
#endif

class TestKeySignalTool : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    // ---- the arguments ----

    void aColourGivenAsHexOrAsThreeChannelsGivesTheSameRequest()
    {
        const auto hex = parseArguments({"--key", "F3", "--color", "#ff7800", "--effect", "pulse", "--seconds", "600"}, false);
        QVERIFY2(hex.arguments.has_value(), qPrintable(hex.error));
        const auto channels = parseArguments({"--key=F3", "--red=255", "--green", "120", "--blue", "0", "--effect=pulse", "--seconds=600"}, false);
        QVERIFY2(channels.arguments.has_value(), qPrintable(channels.error));
        for (const Arguments &a : {argsOf(hex), argsOf(channels)}) {
            QCOMPARE(a.verb, Verb::Light);
            QCOMPARE(a.key, QStringLiteral("F3"));
            QCOMPARE(a.red, 255);
            QCOMPARE(a.green, 120);
            QCOMPARE(a.blue, 0);
            QVERIFY(a.pulse);
            QCOMPARE(a.seconds, 600);
        }
    }

    void theEffectIsSteadyByDefault()
    {
        const auto parsed = parseArguments({"--key", "F3", "--color", "#ffffff", "--seconds", "5"}, false);
        QVERIFY(parsed.arguments.has_value());
        QVERIFY(!argsOf(parsed).pulse);
    }

    void aCommandLineThatIsNotWholeIsRefusedWithAReason_data()
    {
        QTest::addColumn<QStringList>("args");
        QTest::newRow("nothing") << QStringList{};
        QTest::newRow("no key") << QStringList{"--color", "#ffffff", "--seconds", "5"};
        QTest::newRow("no colour") << QStringList{"--key", "F3", "--seconds", "5"};
        QTest::newRow("a colour missing a channel") << QStringList{"--key", "F3", "--red", "1", "--green", "2", "--seconds", "5"};
        QTest::newRow("both colour forms") << QStringList{"--key", "F3", "--color", "#ffffff", "--red", "1", "--seconds", "5"};
        QTest::newRow("a channel over 255") << QStringList{"--key", "F3", "--red", "256", "--green", "0", "--blue", "0", "--seconds", "5"};
        QTest::newRow("a negative channel") << QStringList{"--key", "F3", "--red", "-1", "--green", "0", "--blue", "0", "--seconds", "5"};
        QTest::newRow("a channel with a decimal point") << QStringList{"--key", "F3", "--red", "1.5", "--green", "0", "--blue", "0", "--seconds", "5"};
        QTest::newRow("a bad hex colour") << QStringList{"--key", "F3", "--color", "ff7800", "--seconds", "5"};
        QTest::newRow("no time") << QStringList{"--key", "F3", "--color", "#ffffff"};
        QTest::newRow("a time of zero") << QStringList{"--key", "F3", "--color", "#ffffff", "--seconds", "0"};
        QTest::newRow("longer than a day") << QStringList{"--key", "F3", "--color", "#ffffff", "--seconds", "86401"};
        QTest::newRow("a time that is not a number") << QStringList{"--key", "F3", "--color", "#ffffff", "--seconds", "soon"};
        QTest::newRow("an effect that does not exist") << QStringList{"--key", "F3", "--color", "#ffffff", "--effect", "rainbow", "--seconds", "5"};
        QTest::newRow("the same option twice") << QStringList{"--key", "F3", "--key", "F4", "--color", "#ffffff", "--seconds", "5"};
        QTest::newRow("an option with no value") << QStringList{"--key"};
        QTest::newRow("an unknown option") << QStringList{"--key", "F3", "--color", "#ffffff", "--seconds", "5", "--loud"};
        QTest::newRow("a key name that is too long") << QStringList{"--key", QString(33, 'a'), "--color", "#ffffff", "--seconds", "5"};
        QTest::newRow("a key name with a control character") << QStringList{"--key", QStringLiteral("F3\n"), "--color", "#ffffff", "--seconds", "5"};
        QTest::newRow("help with something else") << QStringList{"--help", "--key", "F3"};
    }
    void aCommandLineThatIsNotWholeIsRefusedWithAReason()
    {
        QFETCH(const QStringList, args);
        const auto parsed = parseArguments(args, false);
        QVERIFY(!parsed.arguments.has_value());
        QVERIFY(!parsed.error.isEmpty());
    }

    void theBoundsOfTheTimeAreTheOnesOfGalleond()
    {
        QVERIFY(parseArguments({"--key", "F3", "--color", "#ffffff", "--seconds", "1"}, false).arguments.has_value());
        QVERIFY(parseArguments({"--key", "F3", "--color", "#ffffff", "--seconds", "86400"}, false).arguments.has_value());
    }

    void helpStandsAloneAndSaysWhichSystemItIsFor()
    {
        const auto help = parseArguments({"--help"}, false);
        QVERIFY(help.arguments.has_value());
        QCOMPARE(argsOf(help).verb, Verb::Help);
        QVERIFY(!usage(false).contains(QStringLiteral("--clear")));
        QVERIFY(usage(true).contains(QStringLiteral("--clear-all")));
        QVERIFY(usage(true).contains(QStringLiteral("--client")));
    }

    void onLinuxThereIsNoClearAndNoClientBecauseASignalBelongsToTheConnection()
    {
        for (const QStringList &args : {QStringList{"--clear", "--key", "F3"}, QStringList{"--clear-all"},
                                        QStringList{"--client", "x", "--key", "F3", "--color", "#ffffff", "--seconds", "5"}}) {
            const auto parsed = parseArguments(args, false);
            QVERIFY2(!parsed.arguments.has_value(), qPrintable(args.join(' ')));
            QVERIFY(!parsed.error.isEmpty());
        }
    }

    void onWindowsClearAndClearAllAndTheClientNameExist()
    {
        const auto clear = parseArguments({"--clear", "--key", "F3", "--client", "assistant"}, true);
        QVERIFY2(clear.arguments.has_value(), qPrintable(clear.error));
        QCOMPARE(argsOf(clear).verb, Verb::Clear);
        QCOMPARE(argsOf(clear).key, QStringLiteral("F3"));
        QCOMPARE(argsOf(clear).client, QStringLiteral("assistant"));
        const auto clearAll = parseArguments({"--clear-all", "--client", "assistant"}, true);
        QVERIFY2(clearAll.arguments.has_value(), qPrintable(clearAll.error));
        QCOMPARE(argsOf(clearAll).verb, Verb::ClearAll);
        // The clear verbs take nothing else, and not both together.
        QVERIFY(!parseArguments({"--clear", "--key", "F3", "--seconds", "5"}, true).arguments.has_value());
        QVERIFY(!parseArguments({"--clear-all", "--key", "F3"}, true).arguments.has_value());
        QVERIFY(!parseArguments({"--clear", "--clear-all", "--key", "F3"}, true).arguments.has_value());
        QVERIFY(!parseArguments({"--clear"}, true).arguments.has_value()); // which key?
        QVERIFY(!parseArguments({"--clear-all", "--client", QString(65, 'a')}, true).arguments.has_value());
        QVERIFY(parseArguments({"--clear-all", "--client", QString(64, 'a')}, true).arguments.has_value());
    }

    // ---- what is sent to the local endpoint (Windows), tested here against the real server of galleond ----

    void theRequestOfTheArgumentsIsWhatTheEndpointTakes()
    {
        const auto light = parseArguments({"--key", "F3", "--color", "#ff7800", "--effect", "pulse", "--seconds", "600", "--client", "assistant"}, true);
        QVERIFY(light.arguments.has_value());
        const galleond::LocalCommand command = commandOf(argsOf(light));
        // What the server of galleond parses back: the same request.
        const auto reparsed = galleond::parseLocalCommand(galleond::encodeLocalCommand(command));
        QVERIFY(reparsed.has_value());
        const galleond::LocalCommand parsed = reparsed.value_or(galleond::LocalCommand{});
        QCOMPARE(parsed.action, galleond::LocalAction::KeySignal);
        QCOMPARE(parsed.client, QStringLiteral("assistant"));
        QCOMPARE(parsed.key, QStringLiteral("F3"));
        QCOMPARE(parsed.red, 255);
        QCOMPARE(parsed.green, 120);
        QCOMPARE(parsed.effect, QStringLiteral("pulse"));
        QCOMPARE(parsed.seconds, 600.0);
        const auto clear = parseArguments({"--clear", "--key", "F3"}, true);
        QCOMPARE(commandOf(argsOf(clear)).action, galleond::LocalAction::KeySignalClear);
        const auto clearAll = parseArguments({"--clear-all"}, true);
        QCOMPARE(commandOf(argsOf(clearAll)).action, galleond::LocalAction::KeySignalClearAll);
        QCOMPARE(commandOf(argsOf(clearAll)).client, QStringLiteral("galleon-key-signal"));
    }

    void aRequestReachesTheServerAndIsAnsweredOk()
    {
        const QString name = QStringLiteral("galleon-keysignal-") + QUuid::createUuid().toString(QUuid::Id128);
        galleond::LocalControl server;
        QVERIFY(server.listen(name));
        const QSignalSpy requested(&server, &galleond::LocalControl::keySignalRequested);
        const auto light = parseArguments({"--key", "F3", "--color", "#0000ff", "--seconds", "30"}, true);
        // The tool waits for the answer in its own socket calls (it is a program of its own): run it from another thread so that this one serves the endpoint.
        QString answer = QStringLiteral("not run");
        const Arguments sent = argsOf(light);
        std::unique_ptr<QThread> client(QThread::create([&answer, &name, &sent] { answer = sendToEndpoint(name, commandOf(sent), 3000); }));
        client->start();
        QTRY_VERIFY_WITH_TIMEOUT(client->isFinished(), 5000);
        QVERIFY2(answer.isEmpty(), qPrintable(answer));
        QCOMPARE(requested.size(), 1);
        QCOMPARE(requested.constFirst().at(1).toString(), QStringLiteral("F3"));
        QCOMPARE(requested.constFirst().at(4).toInt(), 255);
    }

    void anEndpointThatAnswersAnythingButOkIsTold()
    {
        const QString name = QStringLiteral("galleon-keysignal-odd-") + QUuid::createUuid().toString(QUuid::Id128);
        QLocalServer server;
        QVERIFY(server.listen(name));
        QObject::connect(&server, &QLocalServer::newConnection, &server, [&server] {
            QLocalSocket *socket = server.nextPendingConnection();
            QObject::connect(socket, &QLocalSocket::readyRead, socket, [socket] {
                socket->readAll();
                socket->write("{\"ok\":false}\n");
                socket->disconnectFromServer();
            });
            QObject::connect(socket, &QLocalSocket::disconnected, socket, &QObject::deleteLater);
        });
        const auto light = parseArguments({"--key", "F3", "--color", "#0000ff", "--seconds", "30"}, true);
        QString answer;
        const Arguments sent = argsOf(light);
        std::unique_ptr<QThread> client(QThread::create([&answer, &name, &sent] { answer = sendToEndpoint(name, commandOf(sent), 3000); }));
        client->start();
        QTRY_VERIFY_WITH_TIMEOUT(client->isFinished(), 5000);
        QVERIFY(!answer.isEmpty());
    }

    void withoutGalleondTheEndpointSaysSoAndTheToolDoesNotWait()
    {
        const QString name = QStringLiteral("galleon-keysignal-nobody-") + QUuid::createUuid().toString(QUuid::Id128);
        const auto light = parseArguments({"--key", "F3", "--color", "#0000ff", "--seconds", "30"}, true);
        QVERIFY(!sendToEndpoint(name, commandOf(argsOf(light)), 500).isEmpty());
    }

#ifndef Q_OS_WIN
    // ---- Linux: the signal held on the bus ----

    void aSignalIsAskedOfGalleondAndTakenAwayOnRelease()
    {
        const QString connectionName = QStringLiteral("fake-galleond-for-the-held-signal");
        QDBusConnection bus = QDBusConnection::connectToBus(QDBusConnection::SessionBus, connectionName);
        FakeKeySignals fake;
        QVERIFY(bus.registerObject("/org/galleoncore/KeySignals", &fake, QDBusConnection::ExportAllSlots));
        const QString service = QStringLiteral("org.galleoncore.KeySignalsToolTest");
        QVERIFY(bus.registerService(service));
        const auto cleanUp = qScopeGuard([&] {
            bus.unregisterObject("/org/galleoncore/KeySignals");
            bus.unregisterService(service);
            QDBusConnection::disconnectFromBus(connectionName);
        });
        HeldSignal held(QDBusConnection::sessionBus(), service);
        const auto light = parseArguments({"--key", "F3", "--color", "#ff7800", "--effect", "pulse", "--seconds", "600"}, false);
        QVERIFY(light.arguments.has_value());
        QVERIFY(held.light(argsOf(light)).isEmpty());
        QVERIFY(held.isLit());
        QCOMPARE(fake.sets.size(), 1);
        QCOMPARE(fake.sets.constFirst().key, QStringLiteral("F3"));
        QCOMPARE(fake.sets.constFirst().green, 120);
        QCOMPARE(fake.sets.constFirst().effect, QStringLiteral("pulse"));
        QCOMPARE(fake.sets.constFirst().seconds, 600.0);
        QCOMPARE(fake.clears.size(), 0); // nothing is taken away while it is held
        held.release();
        QVERIFY(!held.isLit());
        QCOMPARE(fake.clears, QStringList{QStringLiteral("F3")});
        held.release(); // a second release does nothing
        QCOMPARE(fake.clears.size(), 1);
    }

    void aRefusalOfGalleondIsReturnedInItsOwnWordsAndNothingIsHeld()
    {
        const QString connectionName = QStringLiteral("fake-galleond-for-the-refused-signal");
        QDBusConnection bus = QDBusConnection::connectToBus(QDBusConnection::SessionBus, connectionName);
        FakeKeySignals fake;
        fake.answer = QStringLiteral("Other programs are not allowed to light keys: the person allows it in Bosun.");
        QVERIFY(bus.registerObject("/org/galleoncore/KeySignals", &fake, QDBusConnection::ExportAllSlots));
        const QString service = QStringLiteral("org.galleoncore.KeySignalsToolRefusal");
        QVERIFY(bus.registerService(service));
        const auto cleanUp = qScopeGuard([&] {
            bus.unregisterObject("/org/galleoncore/KeySignals");
            bus.unregisterService(service);
            QDBusConnection::disconnectFromBus(connectionName);
        });
        HeldSignal held(QDBusConnection::sessionBus(), service);
        const auto light = parseArguments({"--key", "F3", "--color", "#ff7800", "--seconds", "60"}, false);
        QCOMPARE(held.light(argsOf(light)), fake.answer);
        QVERIFY(!held.isLit());
        held.release();
        QCOMPARE(fake.clears.size(), 0); // there was nothing to take away
    }

    void withoutGalleondOnTheBusTheToolSaysSo()
    {
        HeldSignal held(QDBusConnection::sessionBus(), QStringLiteral("org.galleoncore.NobodyHomeForTheTool"));
        const auto light = parseArguments({"--key", "F3", "--color", "#ff7800", "--seconds", "60"}, false);
        const QString reason = held.light(argsOf(light));
        QVERIFY(!reason.isEmpty());
        QVERIFY(!held.isLit());
    }

#ifdef KEY_SIGNAL_TOOL
    // ---- the program itself, against a fake galleond on the bus of the test ----

    void theProgramLightsTheKeyHoldsItAndTakesItAwayWhenStopped()
    {
        const QString connectionName = QStringLiteral("fake-galleond-for-the-program");
        QDBusConnection bus = QDBusConnection::connectToBus(QDBusConnection::SessionBus, connectionName);
        FakeKeySignals fake;
        QVERIFY(bus.registerObject("/org/galleoncore/KeySignals", &fake, QDBusConnection::ExportAllSlots));
        QVERIFY(bus.registerService(QStringLiteral("org.galleoncore.Deck")));
        const auto cleanUp = qScopeGuard([&] {
            bus.unregisterObject("/org/galleoncore/KeySignals");
            bus.unregisterService(QStringLiteral("org.galleoncore.Deck"));
            QDBusConnection::disconnectFromBus(connectionName);
        });
        QProcess program;
        program.start(QStringLiteral(KEY_SIGNAL_TOOL), {"--key", "F3", "--color", "#e53935", "--effect", "pulse", "--seconds", "300"});
        QVERIFY(program.waitForStarted(5000));
        QTRY_COMPARE_WITH_TIMEOUT(fake.sets.size(), 1, 10000);
        QCOMPARE(fake.sets.constFirst().key, QStringLiteral("F3"));
        QCOMPARE(fake.sets.constFirst().red, 0xe5);
        QCOMPARE(fake.sets.constFirst().effect, QStringLiteral("pulse"));
        QTest::qWait(300);
        QCOMPARE(program.state(), QProcess::Running); // it holds the signal: its connection is the signal
        QCOMPARE(fake.clears.size(), 0);
        program.terminate(); // what Ctrl-C and kill do
        QTRY_VERIFY_WITH_TIMEOUT(program.state() == QProcess::NotRunning, 10000);
        QCOMPARE(program.exitStatus(), QProcess::NormalExit);
        QCOMPARE(program.exitCode(), 0);
        QTRY_COMPARE_WITH_TIMEOUT(fake.clears, QStringList{QStringLiteral("F3")}, 5000);
        // It says how to take the key away, and that includes `kill`, which is what stops it when it runs in the background (Ctrl-C does not).
        QVERIFY2(QString::fromUtf8(program.readAllStandardOutput()).contains(QStringLiteral("Lit F3 for 300 s. Ctrl-C or kill takes it away.")),
                 "the message that says how to stop it is missing");
    }

    void theProgramEndsByItselfWhenTheTimeIsOverAndTakesTheSignalAway()
    {
        const QString connectionName = QStringLiteral("fake-galleond-for-the-short-program");
        QDBusConnection bus = QDBusConnection::connectToBus(QDBusConnection::SessionBus, connectionName);
        FakeKeySignals fake;
        QVERIFY(bus.registerObject("/org/galleoncore/KeySignals", &fake, QDBusConnection::ExportAllSlots));
        QVERIFY(bus.registerService(QStringLiteral("org.galleoncore.Deck")));
        const auto cleanUp = qScopeGuard([&] {
            bus.unregisterObject("/org/galleoncore/KeySignals");
            bus.unregisterService(QStringLiteral("org.galleoncore.Deck"));
            QDBusConnection::disconnectFromBus(connectionName);
        });
        QProcess program;
        program.start(QStringLiteral(KEY_SIGNAL_TOOL), {"--key", "F2", "--color", "#00ff00", "--seconds", "1"});
        QVERIFY(program.waitForStarted(5000));
        QTRY_VERIFY_WITH_TIMEOUT(program.state() == QProcess::NotRunning, 15000);
        QCOMPARE(program.exitCode(), 0);
        QCOMPARE(fake.sets.size(), 1);
        QTRY_COMPARE_WITH_TIMEOUT(fake.clears, QStringList{QStringLiteral("F2")}, 5000);
    }

    void theProgramExitsWithStatus2AndTheSentenceOfGalleondWhenItRefuses()
    {
        const QString connectionName = QStringLiteral("fake-galleond-for-the-refusing-program");
        QDBusConnection bus = QDBusConnection::connectToBus(QDBusConnection::SessionBus, connectionName);
        FakeKeySignals fake;
        fake.answer = QStringLiteral("The lighting is off: a signal is only shown while it is on.");
        QVERIFY(bus.registerObject("/org/galleoncore/KeySignals", &fake, QDBusConnection::ExportAllSlots));
        QVERIFY(bus.registerService(QStringLiteral("org.galleoncore.Deck")));
        const auto cleanUp = qScopeGuard([&] {
            bus.unregisterObject("/org/galleoncore/KeySignals");
            bus.unregisterService(QStringLiteral("org.galleoncore.Deck"));
            QDBusConnection::disconnectFromBus(connectionName);
        });
        QProcess program;
        program.start(QStringLiteral(KEY_SIGNAL_TOOL), {"--key", "F3", "--color", "#e53935", "--seconds", "60"});
        QVERIFY(program.waitForStarted(5000));
        QTRY_VERIFY_WITH_TIMEOUT(program.state() == QProcess::NotRunning, 15000);
        QCOMPARE(program.exitCode(), 2);
        QVERIFY(QString::fromUtf8(program.readAllStandardError()).contains(fake.answer));
        QCOMPARE(fake.clears.size(), 0);
    }

    void aWrongCommandLineExitsWithStatus1AndSaysWhy()
    {
        QProcess program;
        program.start(QStringLiteral(KEY_SIGNAL_TOOL), {"--key", "F3", "--seconds", "60"}); // no colour
        QVERIFY(program.waitForFinished(5000));
        QCOMPARE(program.exitCode(), 1);
        QVERIFY(QString::fromUtf8(program.readAllStandardError()).contains(QStringLiteral("colour")));
    }

    void helpExitsWithStatus0AndPrintsTheUsage()
    {
        QProcess program;
        program.start(QStringLiteral(KEY_SIGNAL_TOOL), {"--help"});
        QVERIFY(program.waitForFinished(5000));
        QCOMPARE(program.exitCode(), 0);
        QVERIFY(QString::fromUtf8(program.readAllStandardOutput()).contains(QStringLiteral("--seconds")));
    }
#endif
#endif
};

QTEST_MAIN(TestKeySignalTool)
#include "test_keysignaltool.moc"
