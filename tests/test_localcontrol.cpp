// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later
#include "LocalControl.h"
#include <QtTest>
#include <QLocalSocket>
#include <QUuid>
#include <memory>

using namespace galleond;
class TestLocalControl : public QObject
{
    Q_OBJECT
private slots:
    void refusesUnexpectedCommands()
    {
        for (const QByteArray &frame : {QByteArray{}, QByteArray("[]"), QByteArray(R"({"action":"exec"})"),
                 QByteArray(R"({"action":"stop","extra":true})"), QByteArray(R"({"action":"wallpaper","name":1})"),
                 QByteArray(R"({"action":"wallpaper","name":""})"), QByteArray(4097, 'x')})
            QVERIFY(!parseLocalCommand(frame).has_value());
    }
    void gamingCommandSelectsOnlyTheGamingPage()
    {
        const auto command = parseLocalCommand(encodeLocalCommand({LocalAction::Gaming, {}}));
        QVERIFY(command.has_value());
        QCOMPARE(command.value_or(LocalCommand{}).action, LocalAction::Gaming);
        QVERIFY(!parseLocalCommand(R"({"action":"gaming","page":"arbitrary"})"));
        const QString name = QStringLiteral("galleon-gaming-") + QUuid::createUuid().toString(QUuid::Id128);
        LocalControl server; QVERIFY(server.listen(name));
        const QSignalSpy requested(&server, &LocalControl::gamingRequested);
        sendLocalCommand(name, {LocalAction::Gaming, {}}, &server);
        QTRY_COMPARE(requested.size(), 1);
    }
    void themeCommandCarriesOnlyAPageName()
    {
        const auto command = parseLocalCommand(encodeLocalCommand({LocalAction::Theme, {}, QStringLiteral("discord-2")}));
        QVERIFY(command.has_value());
        QCOMPARE(command.value_or(LocalCommand{}).action, LocalAction::Theme);
        QCOMPARE(command.value_or(LocalCommand{}).page, QStringLiteral("discord-2"));
        for (const QByteArray &frame : {QByteArray(R"({"action":"theme"})"), QByteArray(R"({"action":"theme","page":1})"),
                 QByteArray(R"({"action":"theme","page":"../etc"})"), QByteArray(R"({"action":"theme","page":"Upper"})"),
                 QByteArray(R"({"action":"theme","page":""})"), QByteArray(R"({"action":"theme","page":"a","name":"b"})")})
            QVERIFY(!parseLocalCommand(frame).has_value());
        const QString name = QStringLiteral("galleon-theme-") + QUuid::createUuid().toString(QUuid::Id128);
        LocalControl server; QVERIFY(server.listen(name));
        const QSignalSpy requested(&server, &LocalControl::themeRequested);
        sendLocalCommand(name, {LocalAction::Theme, {}, QStringLiteral("mine")}, &server);
        QTRY_COMPARE(requested.size(), 1);
        QCOMPARE(requested.constFirst().constFirst().toString(), QStringLiteral("mine"));
    }
    void keyboardBrightnessCommandCarriesOnlyAWholeNumberFromZeroToAThousand()
    {
        const auto command = parseLocalCommand(encodeLocalCommand({LocalAction::KeyboardBrightness, {}, {}, 600}));
        QVERIFY(command.has_value());
        QCOMPARE(command.value_or(LocalCommand{}).action, LocalAction::KeyboardBrightness);
        QCOMPARE(command.value_or(LocalCommand{}).brightness, 600);
        for (const QByteArray &frame : {QByteArray(R"({"action":"keyboard-brightness"})"), QByteArray(R"({"action":"keyboard-brightness","value":"5"})"),
                 QByteArray(R"({"action":"keyboard-brightness","value":-1})"), QByteArray(R"({"action":"keyboard-brightness","value":1001})"),
                 QByteArray(R"({"action":"keyboard-brightness","value":3.5})"), QByteArray(R"({"action":"keyboard-brightness","value":1,"x":2})")})
            QVERIFY(!parseLocalCommand(frame).has_value());
        const QString name = QStringLiteral("galleon-brightness-") + QUuid::createUuid().toString(QUuid::Id128);
        LocalControl server; QVERIFY(server.listen(name));
        const QSignalSpy requested(&server, &LocalControl::keyboardBrightnessRequested);
        sendLocalCommand(name, {LocalAction::KeyboardBrightness, {}, {}, 300}, &server);
        QTRY_COMPARE(requested.size(), 1);
        QCOMPARE(requested.constFirst().constFirst().toInt(), 300);
    }
    void theLightingCommandsRoundTripThroughTheirEncoding()
    {
        LocalCommand enable{LocalAction::LightingEnable, {}, {}};
        enable.on = true;
        const auto parsed = parseLocalCommand(encodeLocalCommand(enable));
        if (!parsed)
            QFAIL("the enable command was not parsed");
        QCOMPARE(parsed->action, LocalAction::LightingEnable);
        QVERIFY(parsed->on);
        LocalCommand colour{LocalAction::LightingColor, {}, {}};
        colour.red = 10;
        colour.green = 20;
        colour.blue = 255;
        const auto parsedColour = parseLocalCommand(encodeLocalCommand(colour));
        if (!parsedColour)
            QFAIL("the colour command was not parsed");
        QCOMPARE(std::tuple(parsedColour->red, parsedColour->green, parsedColour->blue), std::tuple(10, 20, 255));
    }

    void aSingleLedCommandRoundTripsThroughItsEncoding()
    {
        LocalCommand led{LocalAction::LightingLed, {}, {}};
        led.led = 58;
        led.red = 255;
        led.green = 0;
        led.blue = 7;
        const auto parsed = parseLocalCommand(encodeLocalCommand(led));
        if (!parsed)
            QFAIL("the single LED command was not parsed");
        QCOMPARE(parsed->action, LocalAction::LightingLed);
        QCOMPARE(std::tuple(parsed->led, parsed->red, parsed->green, parsed->blue), std::tuple(58, 255, 0, 7));
    }

    void aSingleLedCommandWithAnythingOutOfShapeIsRefused()
    {
        for (const char *frame : {R"({"action":"lighting-led","led":1,"red":1,"green":2})", R"({"action":"lighting-led","red":1,"green":2,"blue":3})",
                                  R"({"action":"lighting-led","led":146,"red":0,"green":0,"blue":0})", R"({"action":"lighting-led","led":-1,"red":0,"green":0,"blue":0})",
                                  R"({"action":"lighting-led","led":1.5,"red":0,"green":0,"blue":0})", R"({"action":"lighting-led","led":"1","red":0,"green":0,"blue":0})",
                                  R"({"action":"lighting-led","led":1,"red":256,"green":0,"blue":0})",
                                  R"({"action":"lighting-led","led":1,"red":1,"green":2,"blue":3,"extra":4})"})
            QVERIFY2(!parseLocalCommand(frame), frame);
        QVERIFY(parseLocalCommand(R"({"action":"lighting-led","led":145,"red":0,"green":0,"blue":0})").has_value()); // the last LED of the frame
    }

    void aCommandForOneColourPerLedRoundTripsThroughItsEncoding()
    {
        LocalCommand command{LocalAction::LightingLeds, {}, {}};
        command.ledValues = {58, 255, 0, 0, 4, 0, 255, 0, 145, 1, 2, 3};
        const auto parsed = parseLocalCommand(encodeLocalCommand(command));
        if (!parsed)
            QFAIL("the command was not parsed");
        QCOMPARE(parsed->action, LocalAction::LightingLeds);
        QCOMPARE(parsed->ledValues, command.ledValues);
    }

    void theBiggestListOfLedsFitsInOneFrame()
    {
        LocalCommand command{LocalAction::LightingLeds, {}, {}};
        for (int led = 0; led < 146; ++led)
            command.ledValues << led << 255 << 255 << 255;
        const QByteArray frame = encodeLocalCommand(command);
        QVERIFY2(frame.size() <= 4096, "the biggest list has to fit in the 4096 bytes of one frame");
        const auto parsed = parseLocalCommand(frame);
        QVERIFY(parsed.has_value());
        QCOMPARE(parsed.value_or(LocalCommand{}).ledValues.size(), 146 * 4);
    }

    void aListOfLedsWithAnythingOutOfShapeIsRefused()
    {
        for (const char *frame : {R"({"action":"lighting-leds"})", R"({"action":"lighting-leds","leds":[]})", R"({"action":"lighting-leds","leds":5})",
                                  R"({"action":"lighting-leds","leds":[[1,2,3]]})", R"({"action":"lighting-leds","leds":[[1,2,3,4,5]]})",
                                  R"({"action":"lighting-leds","leds":[1,2,3,4]})", R"({"action":"lighting-leds","leds":[[146,0,0,0]]})",
                                  R"({"action":"lighting-leds","leds":[[-1,0,0,0]]})", R"({"action":"lighting-leds","leds":[[1,256,0,0]]})",
                                  R"({"action":"lighting-leds","leds":[[1,0,-1,0]]})", R"({"action":"lighting-leds","leds":[[1.5,0,0,0]]})",
                                  R"({"action":"lighting-leds","leds":[["1",0,0,0]]})", R"({"action":"lighting-leds","leds":[[1,0,0,null]]})",
                                  R"({"action":"lighting-leds","leds":[[1,0,0,0]],"extra":1})"})
            QVERIFY2(!parseLocalCommand(frame), frame);
        QByteArray tooMany = R"({"action":"lighting-leds","leds":[)";
        for (int i = 0; i < 147; ++i)
            tooMany += (i != 0 ? "," : "") + QByteArray("[3,1,1,1]");
        tooMany += "]}";
        QVERIFY(!parseLocalCommand(tooMany)); // more groups than LEDs
        QVERIFY(parseLocalCommand(R"({"action":"lighting-leds","leds":[[145,255,255,255]]})").has_value());
    }

    void aLightingCommandWithAnythingOutOfShapeIsRefused()
    {
        for (const char *frame : {R"({"action":"lighting-enable"})", R"({"action":"lighting-enable","on":1})", R"({"action":"lighting-enable","on":"true"})",
                                  R"({"action":"lighting-enable","on":true,"x":1})",
                                  R"({"action":"lighting-color","red":1,"green":2})", R"({"action":"lighting-color","red":256,"green":0,"blue":0})",
                                  R"({"action":"lighting-color","red":-1,"green":0,"blue":0})", R"({"action":"lighting-color","red":1.5,"green":0,"blue":0})",
                                  R"({"action":"lighting-color","red":"1","green":0,"blue":0})",
                                  R"({"action":"lighting-color","red":1,"green":2,"blue":3,"extra":4})"})
            QVERIFY2(!parseLocalCommand(frame), frame);
    }

    void aKeySignalRoundTripsThroughItsEncodingWithItsWholeShape()
    {
        LocalCommand sent{LocalAction::KeySignal, {}, {}};
        sent.client = QStringLiteral("desk helper");
        sent.key = QStringLiteral("Left Control");
        sent.red = 255;
        sent.green = 120;
        sent.blue = 0;
        sent.effect = QStringLiteral("pulse");
        sent.seconds = 600.5;
        const auto parsed = parseLocalCommand(encodeLocalCommand(sent));
        QVERIFY(parsed.has_value());
        const LocalCommand received = parsed.value_or(LocalCommand{});
        QCOMPARE(received.action, LocalAction::KeySignal);
        QCOMPARE(received.client, sent.client);
        QCOMPARE(received.key, sent.key);
        QCOMPARE(received.red, 255);
        QCOMPARE(received.green, 120);
        QCOMPARE(received.blue, 0);
        QCOMPARE(received.effect, QStringLiteral("pulse"));
        QCOMPARE(received.seconds, 600.5);
    }

    void aKeySignalWithAnythingOutOfShapeIsRefused()
    {
        const QString good = QStringLiteral(R"({"action":"key-signal","client":"a","key":"F3","red":1,"green":2,"blue":3,"effect":"steady","seconds":60})");
        QVERIFY(parseLocalCommand(good.toUtf8()).has_value());
        const auto changed = [&good](const char *what, const char *with) {
            QString text = good;
            text.replace(QString::fromLatin1(what), QString::fromLatin1(with));
            return text.toUtf8();
        };
        const QList<QByteArray> frames{
            changed(R"("client":"a")", R"("client":"")"), changed(R"("client":"a")", R"("client":7)"),
            changed(R"("client":"a")", R"("client":"café")"), changed(R"("client":"a")", R"("client":"two\nlines")"),
            changed(R"("client":"a")", QStringLiteral(R"("client":"%1")").arg(QString(65, QLatin1Char('a'))).toLatin1().constData()),
            changed(R"("key":"F3")", R"("key":"")"), changed(R"("key":"F3")", R"("key":3)"),
            changed(R"("key":"F3")", R"("key":"a name that is much longer than any key has")"),
            changed(R"("red":1)", R"("red":256)"), changed(R"("green":2)", R"("green":-1)"), changed(R"("blue":3)", R"("blue":1.5)"),
            changed(R"("blue":3)", R"("blue":"3")"),
            changed(R"("effect":"steady")", R"("effect":"rainbow")"), changed(R"("effect":"steady")", R"("effect":"Steady")"),
            changed(R"("effect":"steady")", R"("effect":1)"),
            changed(R"("seconds":60)", R"("seconds":0.5)"), changed(R"("seconds":60)", R"("seconds":0)"), changed(R"("seconds":60)", R"("seconds":-5)"),
            changed(R"("seconds":60)", R"("seconds":86401)"), changed(R"("seconds":60)", R"("seconds":"60")"), changed(R"("seconds":60)", R"("seconds":null)"),
            changed(R"("seconds":60})", R"("seconds":60,"extra":1})"), // an extra key
            changed(R"(,"seconds":60)", R"()"),                          // a missing key
            QByteArray(R"({"action":"key-signal"})")};
        for (const QByteArray &frame : frames)
            QVERIFY2(!parseLocalCommand(frame).has_value(), frame.constData());
        QVERIFY(parseLocalCommand(changed(R"("client":"a")", QStringLiteral(R"("client":"%1")").arg(QString(64, QLatin1Char('a'))).toLatin1().constData())).has_value());
        QVERIFY(parseLocalCommand(changed(R"("seconds":60)", R"("seconds":86400)")).has_value());
        QVERIFY(parseLocalCommand(changed(R"("seconds":60)", R"("seconds":1)")).has_value());
    }

    void clearingAKeySignalCarriesOnlyWhoAsksAndTheKey()
    {
        QVERIFY(parseLocalCommand(R"({"action":"key-signal-clear","client":"a","key":"F3"})").has_value());
        QVERIFY(parseLocalCommand(R"({"action":"key-signal-clear-all","client":"a"})").has_value());
        for (const char *frame : {R"({"action":"key-signal-clear","client":"a"})", R"({"action":"key-signal-clear","key":"F3"})",
                                  R"({"action":"key-signal-clear","client":"a","key":"F3","red":1})", R"({"action":"key-signal-clear-all"})",
                                  R"({"action":"key-signal-clear-all","client":"a","key":"F3"})", R"({"action":"key-signal-clear-all","client":""})"})
            QVERIFY2(!parseLocalCommand(frame).has_value(), frame);
        LocalCommand clear{LocalAction::KeySignalClear, {}, {}};
        clear.client = QStringLiteral("a");
        clear.key = QStringLiteral("F3");
        QCOMPARE(parseLocalCommand(encodeLocalCommand(clear)).value_or(LocalCommand{}).action, LocalAction::KeySignalClear);
        LocalCommand all{LocalAction::KeySignalClearAll, {}, {}};
        all.client = QStringLiteral("a");
        QCOMPARE(parseLocalCommand(encodeLocalCommand(all)).value_or(LocalCommand{}).action, LocalAction::KeySignalClearAll);
    }

    void aKeySignalReachesTheServerAsAskedAndTheEndpointStillRepliesOnlyOk()
    {
        const QString name = QStringLiteral("galleon-signal-") + QUuid::createUuid().toString(QUuid::Id128);
        LocalControl server;
        QVERIFY(server.listen(name));
        const QSignalSpy requested(&server, &LocalControl::keySignalRequested);
        LocalCommand command{LocalAction::KeySignal, {}, {}};
        command.client = QStringLiteral("helper");
        command.key = QStringLiteral("F3");
        command.red = 1;
        command.effect = QStringLiteral("steady");
        command.seconds = 30;
        sendLocalCommand(name, command, &server);
        QTRY_COMPARE(requested.size(), 1);
        QCOMPARE(requested.constFirst().at(0).toString(), QStringLiteral("helper"));
        QCOMPARE(requested.constFirst().at(1).toString(), QStringLiteral("F3"));
        QCOMPARE(requested.constFirst().at(5).toString(), QStringLiteral("steady"));
        QCOMPARE(requested.constFirst().at(6).toDouble(), 30.0);
    }

    void aKeyboardBrightnessReadCarriesNothingElse()
    {
        const auto command = parseLocalCommand(encodeLocalCommand({LocalAction::KeyboardBrightnessRead, {}, {}}));
        QVERIFY(command.has_value());
        QCOMPARE(command.value_or(LocalCommand{}).action, LocalAction::KeyboardBrightnessRead);
        QVERIFY(!parseLocalCommand(QByteArray(R"({"action":"keyboard-brightness-read","value":5})")).has_value());
        const QString name = QStringLiteral("galleon-brightness-read-") + QUuid::createUuid().toString(QUuid::Id128);
        LocalControl server; QVERIFY(server.listen(name));
        const QSignalSpy requested(&server, &LocalControl::keyboardBrightnessReadRequested);
        sendLocalCommand(name, {LocalAction::KeyboardBrightnessRead, {}, {}}, &server);
        QTRY_COMPARE(requested.size(), 1);
    }
    void roundTripWallpaper()
    {
        const QString name = QString::fromUtf8("personal/caf\xc3\xa9.jpg");
        const auto command = parseLocalCommand(encodeLocalCommand({LocalAction::Wallpaper, name}));
        QVERIFY(command.has_value());
        QCOMPARE(command.value_or(LocalCommand{}).wallpaper, name);
        QCOMPARE(command.value_or(LocalCommand{}).action, LocalAction::Wallpaper);
    }
    void appliesOneCommandAndLeavesSecondInstanceAlone()
    {
        const QString name = QStringLiteral("galleon-test-") + QUuid::createUuid().toString(QUuid::Id128);
        LocalControl server;
        QVERIFY(server.listen(name));
        LocalControl second;
        QVERIFY(!second.listen(name));
        const auto requests = std::make_unique<QSignalSpy>(&server, &LocalControl::wallpaperRequested);
        sendLocalCommand(name, {LocalAction::Wallpaper, QStringLiteral("personal/example.jpg")}, &server);
        QTRY_COMPARE(requests->size(), 1);
        QCOMPARE(requests->first().first().toString(), QStringLiteral("personal/example.jpg"));
    }
    void invalidInputCannotStopDaemon()
    {
        const QString name = QStringLiteral("galleon-test-") + QUuid::createUuid().toString(QUuid::Id128);
        LocalControl server;
        QVERIFY(server.listen(name));
        const auto stops = std::make_unique<QSignalSpy>(&server, &LocalControl::stopRequested);
        QLocalSocket client;
        client.connectToServer(name);
        QVERIFY(client.waitForConnected());
        client.write("{\"action\":\"stop\",\"extra\":true}\n");
        QTRY_COMPARE(client.state(), QLocalSocket::UnconnectedState);
        QCOMPARE(stops->size(), 0);
    }
};
QTEST_GUILESS_MAIN(TestLocalControl)
#include "test_localcontrol.moc"
