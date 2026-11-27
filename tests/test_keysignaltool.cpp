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
// The first request of a call (the only one for a single key); a plain value for the same reason.
galleond::LocalCommand commandOf(const Arguments &arguments)
{
    const auto commands = commandsOf(arguments);
    return commands.isEmpty() ? galleond::LocalCommand{} : commands.constFirst();
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
    int clearAlls = 0;
    QString answer;    // what Set says: "" when accepted, else why not
    QString refuseKey; // when it is not empty, only this key is refused (with `answer`), the others are accepted
public Q_SLOTS:
    QString Set(const QString &key, int red, int green, int blue, const QString &effect, double seconds)
    {
        sets.append({key, red, green, blue, effect, seconds});
        return refuseKey.isEmpty() || key == refuseKey ? answer : QString();
    }
    bool Clear(const QString &key)
    {
        clears.append(key);
        return true;
    }
    bool ClearAll()
    {
        ++clearAlls;
        return true;
    }
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
            QCOMPARE(a.keys, QStringList{QStringLiteral("F3")});
            QCOMPARE(a.red, 255);
            QCOMPARE(a.green, 120);
            QCOMPARE(a.blue, 0);
            QVERIFY(a.pulse);
            QCOMPARE(a.seconds, 600);
        }
    }

    void severalKeysGivenTogetherShareTheColourTheEffectAndTheTime()
    {
        const auto parsed = parseArguments({"--key", "F3,F4,F5", "--color", "#ff7800", "--effect", "pulse", "--seconds", "90"}, false);
        QVERIFY2(parsed.arguments.has_value(), qPrintable(parsed.error));
        QCOMPARE(argsOf(parsed).keys, (QStringList{"F3", "F4", "F5"})); // in the order named
        QCOMPARE(argsOf(parsed).green, 120);
        QVERIFY(argsOf(parsed).pulse);
        QCOMPARE(argsOf(parsed).seconds, 90);
    }

    void thespacesAroundANameAreNotPartOfItButAKeyNameKeepsItsOwn()
    {
        const auto parsed = parseArguments({"--key", "F3, Left Control ,F5", "--color", "#ffffff", "--seconds", "5"}, false);
        QVERIFY2(parsed.arguments.has_value(), qPrintable(parsed.error));
        QCOMPARE(argsOf(parsed).keys, (QStringList{"F3", "Left Control", "F5"}));
    }

    void aListOfKeysThatIsNotWholeIsRefusedWithAReason_data()
    {
        QTest::addColumn<QString>("list");
        QTest::newRow("an empty name between two commas") << QStringLiteral("F3,,F4");
        QTest::newRow("a comma at the end") << QStringLiteral("F3,");
        QTest::newRow("a comma at the start") << QStringLiteral(",F3");
        QTest::newRow("only a comma") << QStringLiteral(",");
        QTest::newRow("a space where a name should be") << QStringLiteral("F3, ,F4");
        QTest::newRow("the same key twice") << QStringLiteral("F3,F4,F3");
        QTest::newRow("the same key twice in another case") << QStringLiteral("F3,f3");
        QTest::newRow("a name of more than 32 characters") << (QStringLiteral("F3,") + QString(33, QLatin1Char('x')));
        QTest::newRow("a control character in a name") << QStringLiteral("F3,F4\x01");
        QTest::newRow("a line break after a name in a list") << QStringLiteral("F3,F4\n");
        QTest::newRow("a tab before a name in a list") << QStringLiteral("F3,\tF4");
        QTest::newRow("more keys than galleond keeps for one program") << QStringList{"F1", "F2", "F3", "F4", "F5", "F6", "F7", "F8", "F9", "F10", "F11", "F12", "Esc", "A", "B", "C", "D"}.join(QLatin1Char(','));
    }

    void aListOfKeysThatIsNotWholeIsRefusedWithAReason()
    {
        QFETCH(const QString, list);
        const auto parsed = parseArguments({"--key", list, "--color", "#ffffff", "--seconds", "5"}, false);
        QVERIFY2(!parsed.arguments.has_value(), qPrintable(list));
        QVERIFY(!parsed.error.isEmpty());
    }

    void theMostKeysACallCanNameIsWhatGalleondKeepsForOneProgram()
    {
        QStringList names;
        for (int i = 0; i < MaxKeysPerCall; ++i)
            names << QStringLiteral("Key%1").arg(i);
        QVERIFY(parseArguments({"--key", names.join(QLatin1Char(',')), "--color", "#ffffff", "--seconds", "5"}, false).arguments.has_value());
        names << QStringLiteral("OneMore");
        QVERIFY(!parseArguments({"--key", names.join(QLatin1Char(',')), "--color", "#ffffff", "--seconds", "5"}, false).arguments.has_value());
        QCOMPARE(MaxKeysPerCall, 16); // MaxSignalsPerClient of KeySignals.h, which this tool does not link: the number is written in both places
    }

    void clearOnWindowsTakesAwayEachKeyOfTheList()
    {
        const auto clear = parseArguments({"--clear", "--key", "F3,F4"}, true);
        QVERIFY2(clear.arguments.has_value(), qPrintable(clear.error));
        QCOMPARE(argsOf(clear).keys, (QStringList{"F3", "F4"}));
        const auto commands = commandsOf(argsOf(clear));
        QCOMPARE(commands.size(), 2);
        QCOMPARE(commands.at(0).action, galleond::LocalAction::KeySignalClear);
        QCOMPARE(commands.at(0).key, QStringLiteral("F3"));
        QCOMPARE(commands.at(1).key, QStringLiteral("F4"));
    }

    void helpMentionsTheListOfKeys()
    {
        QVERIFY(usage(false).contains(QStringLiteral("F3,F4,F5")));
        QVERIFY(usage(true).contains(QStringLiteral("F3,F4,F5")));
    }

    void everyExampleOfTheHelpPutsTheColourInDoubleQuotesAndTheHelpSaysWhy()
    {
        // A bare # starts a comment in bash and PowerShell: the example would be cut at it ("--color needs a value").
        for (const bool windows : {false, true}) {
            const QString text = usage(windows);
            int examples = 0;
            for (const QString &line : text.split(QLatin1Char('\n'))) {
                if (!line.contains(QStringLiteral("galleon-key-signal ")) || !line.contains(QStringLiteral("--color ")))
                    continue;
                ++examples;
                QVERIFY2(line.contains(QStringLiteral("--color \"#")), qPrintable(line));
            }
            QVERIFY(examples >= 2); // the single key and the list
            QVERIFY(text.contains(QStringLiteral("in double quotes")));
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
        QCOMPARE(argsOf(clear).keys, QStringList{QStringLiteral("F3")});
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

    void aCallWithSeveralKeysIsOneRequestForEachInTheOrderNamedAndClearAllIsOne()
    {
        const auto light = parseArguments({"--key", "F3,F4,F5", "--color", "#ff7800", "--effect", "pulse", "--seconds", "600", "--client", "assistant"}, true);
        QVERIFY(light.arguments.has_value());
        const auto commands = commandsOf(argsOf(light));
        QCOMPARE(commands.size(), 3);
        const QStringList wanted{"F3", "F4", "F5"};
        for (qsizetype i = 0; i < commands.size(); ++i) {
            QCOMPARE(commands.at(i).action, galleond::LocalAction::KeySignal);
            QCOMPARE(commands.at(i).key, wanted.at(i));
            QCOMPARE(commands.at(i).client, QStringLiteral("assistant"));
            QCOMPARE(commands.at(i).green, 120);
            QCOMPARE(commands.at(i).effect, QStringLiteral("pulse"));
            QCOMPARE(commands.at(i).seconds, 600);
        }
        const auto clearAll = parseArguments({"--clear-all"}, true);
        QCOMPARE(commandsOf(argsOf(clearAll)).size(), 1); // one request, whatever the keys
        QVERIFY(commandsOf(argsOf(clearAll)).constFirst().key.isEmpty());
    }

    void severalRequestsReachTheServerOneAfterTheOtherInTheOrderNamed()
    {
        const QString name = QStringLiteral("galleon-keysignal-many-") + QUuid::createUuid().toString(QUuid::Id128);
        galleond::LocalControl server;
        QVERIFY(server.listen(name));
        const QSignalSpy requested(&server, &galleond::LocalControl::keySignalRequested);
        const auto light = parseArguments({"--key", "F3,F4,F5", "--color", "#0000ff", "--seconds", "30"}, true);
        QString answer = QStringLiteral("not run");
        const Arguments sent = argsOf(light);
        std::unique_ptr<QThread> client(QThread::create([&answer, &name, &sent] { answer = sendAllToEndpoint(name, commandsOf(sent), 3000); }));
        client->start();
        QTRY_VERIFY_WITH_TIMEOUT(client->isFinished(), 10000);
        QVERIFY2(answer.isEmpty(), qPrintable(answer));
        QCOMPARE(requested.size(), 3);
        QCOMPARE(requested.at(0).at(1).toString(), QStringLiteral("F3"));
        QCOMPARE(requested.at(1).at(1).toString(), QStringLiteral("F4"));
        QCOMPARE(requested.at(2).at(1).toString(), QStringLiteral("F5"));
    }

    void whenARequestFailsTheOnesAfterItAreNotSentAndTheAnswerNamesTheKeyAndHowManyWereTaken()
    {
        const QString name = QStringLiteral("galleon-keysignal-half-") + QUuid::createUuid().toString(QUuid::Id128);
        QLocalServer server;
        QVERIFY(server.listen(name));
        int connections = 0;
        QObject::connect(&server, &QLocalServer::newConnection, &server, [&server, &connections] {
            QLocalSocket *socket = server.nextPendingConnection();
            const bool takes = connections++ == 0; // the first request is taken, the second is not
            QObject::connect(socket, &QLocalSocket::readyRead, socket, [socket, takes] {
                socket->readAll();
                socket->write(takes ? "{\"ok\":true}\n" : "{\"ok\":false}\n");
                socket->disconnectFromServer();
            });
            QObject::connect(socket, &QLocalSocket::disconnected, socket, &QObject::deleteLater);
        });
        const auto light = parseArguments({"--key", "F3,F4,F5", "--color", "#0000ff", "--seconds", "30"}, true);
        QString answer;
        const Arguments sent = argsOf(light);
        std::unique_ptr<QThread> client(QThread::create([&answer, &name, &sent] { answer = sendAllToEndpoint(name, commandsOf(sent), 3000); }));
        client->start();
        QTRY_VERIFY_WITH_TIMEOUT(client->isFinished(), 10000);
        QVERIFY(answer.contains(QStringLiteral("F4"))); // the key whose request failed
        QVERIFY(answer.contains(QStringLiteral("1 of 3"))); // one had been taken before it
        QCOMPARE(connections, 2); // F5 was not tried
    }

    void withoutGalleondEverySendFailsAtTheFirstRequest()
    {
        const QString name = QStringLiteral("galleon-keysignal-none-") + QUuid::createUuid().toString(QUuid::Id128);
        const auto light = parseArguments({"--key", "F3,F4", "--color", "#0000ff", "--seconds", "30"}, true);
        const QString answer = sendAllToEndpoint(name, commandsOf(argsOf(light)), 300);
        QVERIFY(answer.contains(QStringLiteral("F3")));
        QVERIFY(answer.contains(QStringLiteral("0 of 2")));
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

    void severalKeysAreAskedInTheOrderNamedHeldTogetherAndTakenAwayByOneClearAll()
    {
        const QString connectionName = QStringLiteral("fake-galleond-for-the-many-signals");
        QDBusConnection bus = QDBusConnection::connectToBus(QDBusConnection::SessionBus, connectionName);
        FakeKeySignals fake;
        QVERIFY(bus.registerObject("/org/galleoncore/KeySignals", &fake, QDBusConnection::ExportAllSlots));
        const QString service = QStringLiteral("org.galleoncore.KeySignalsToolMany");
        QVERIFY(bus.registerService(service));
        const auto cleanUp = qScopeGuard([&] {
            bus.unregisterObject("/org/galleoncore/KeySignals");
            bus.unregisterService(service);
            QDBusConnection::disconnectFromBus(connectionName);
        });
        HeldSignal held(QDBusConnection::sessionBus(), service);
        const auto light = parseArguments({"--key", "F3,F4,F5", "--color", "#ff7800", "--effect", "pulse", "--seconds", "600"}, false);
        QVERIFY(light.arguments.has_value());
        QVERIFY(held.light(argsOf(light)).isEmpty());
        QVERIFY(held.isLit());
        QCOMPARE(fake.sets.size(), 3);
        for (qsizetype i = 0; i < 3; ++i) { // the same colour, effect and time for each
            QCOMPARE(fake.sets.at(i).key, (QStringList{"F3", "F4", "F5"}).at(i));
            QCOMPARE(fake.sets.at(i).green, 120);
            QCOMPARE(fake.sets.at(i).effect, QStringLiteral("pulse"));
            QCOMPARE(fake.sets.at(i).seconds, 600.0);
        }
        QCOMPARE(fake.clears.size() + fake.clearAlls, 0); // nothing is taken away while they are held
        held.release();
        QVERIFY(!held.isLit());
        QCOMPARE(fake.clearAlls, 1); // one request, not one for each key (galleond takes 20 a second)
        QCOMPARE(fake.clears.size(), 0);
        held.release();
        QCOMPARE(fake.clearAlls, 1); // a second release does nothing
    }

    void whenOneKeyIsRefusedTheOnesAlreadyLitByTheCallAreTakenAwayAndNothingIsHeld()
    {
        const QString connectionName = QStringLiteral("fake-galleond-for-the-half-refused-call");
        QDBusConnection bus = QDBusConnection::connectToBus(QDBusConnection::SessionBus, connectionName);
        FakeKeySignals fake;
        fake.answer = QStringLiteral("This program already lights as many keys as it may.");
        fake.refuseKey = QStringLiteral("F4");
        QVERIFY(bus.registerObject("/org/galleoncore/KeySignals", &fake, QDBusConnection::ExportAllSlots));
        const QString service = QStringLiteral("org.galleoncore.KeySignalsToolHalf");
        QVERIFY(bus.registerService(service));
        const auto cleanUp = qScopeGuard([&] {
            bus.unregisterObject("/org/galleoncore/KeySignals");
            bus.unregisterService(service);
            QDBusConnection::disconnectFromBus(connectionName);
        });
        HeldSignal held(QDBusConnection::sessionBus(), service);
        const auto light = parseArguments({"--key", "F3,F4,F5", "--color", "#ff7800", "--seconds", "60"}, false);
        QCOMPARE(held.light(argsOf(light)), fake.answer); // galleond's own words
        QVERIFY(!held.isLit());
        QCOMPARE(fake.sets.size(), 2); // F5 was not asked
        QCOMPARE(fake.clears, QStringList{QStringLiteral("F3")}); // the one that had been lit goes again
        QCOMPARE(fake.clearAlls, 0);
        held.release();
        QCOMPARE(fake.clears.size(), 1); // nothing is held: nothing more to take away
    }

    void aSecondCallOnAnObjectThatHoldsSignalsIsRefusedAndAsksNothing()
    {
        const QString connectionName = QStringLiteral("fake-galleond-for-the-second-call");
        QDBusConnection bus = QDBusConnection::connectToBus(QDBusConnection::SessionBus, connectionName);
        FakeKeySignals fake;
        QVERIFY(bus.registerObject("/org/galleoncore/KeySignals", &fake, QDBusConnection::ExportAllSlots));
        const QString service = QStringLiteral("org.galleoncore.KeySignalsToolSecond");
        QVERIFY(bus.registerService(service));
        const auto cleanUp = qScopeGuard([&] {
            bus.unregisterObject("/org/galleoncore/KeySignals");
            bus.unregisterService(service);
            QDBusConnection::disconnectFromBus(connectionName);
        });
        HeldSignal held(QDBusConnection::sessionBus(), service);
        const auto light = parseArguments({"--key", "F3,F4", "--color", "#ff7800", "--seconds", "60"}, false);
        QVERIFY(held.light(argsOf(light)).isEmpty());
        QCOMPARE(fake.sets.size(), 2);
        QVERIFY(!held.light(argsOf(light)).isEmpty()); // what is held would be lost track of
        QCOMPARE(fake.sets.size(), 2);
        QVERIFY(held.isLit());
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

    void theProgramLightsSeveralKeysHoldsThemAndTakesThemAllAwayWhenStopped()
    {
        const QString connectionName = QStringLiteral("fake-galleond-for-the-program-with-keys");
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
        program.start(QStringLiteral(KEY_SIGNAL_TOOL), {"--key", "F3,F4,F5", "--color", "#e53935", "--seconds", "300"});
        QVERIFY(program.waitForStarted(5000));
        QTRY_COMPARE_WITH_TIMEOUT(fake.sets.size(), 3, 10000);
        QCOMPARE(fake.sets.at(0).key, QStringLiteral("F3"));
        QCOMPARE(fake.sets.at(1).key, QStringLiteral("F4"));
        QCOMPARE(fake.sets.at(2).key, QStringLiteral("F5"));
        QTest::qWait(300);
        QCOMPARE(program.state(), QProcess::Running);
        program.terminate();
        QTRY_VERIFY_WITH_TIMEOUT(program.state() == QProcess::NotRunning, 10000);
        QCOMPARE(program.exitCode(), 0);
        QTRY_COMPARE_WITH_TIMEOUT(fake.clearAlls, 1, 5000);
        QCOMPARE(fake.clears.size(), 0);
        QVERIFY2(QString::fromUtf8(program.readAllStandardOutput()).contains(QStringLiteral("Lit F3, F4, F5 for 300 s. Ctrl-C or kill takes them away.")),
                 "the message that says how to stop them is missing");
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
